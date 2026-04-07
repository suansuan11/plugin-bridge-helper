#include "AVDemuxer.h"
#include "AVDemuxer.h"

namespace IPCDemo
{
	static AVPacket    g_packet;
	static const char* g_wanted_stream_spec[AVMEDIA_TYPE_NB] = { 0 };

	static inline INT64 get_valid_channel_layout(INT64 channel_layout, int channels)
	{
		if (channel_layout && av_get_channel_layout_nb_channels(channel_layout) == channels)
			return channel_layout;
		else
			return 0;
	}

	AVPacketQueue::AVPacketQueue()
		: m_stop(TRUE)
		, m_serial(0)
		, m_size(0)
		, m_duration(0)
	{
	}

	AVPacketQueue::~AVPacketQueue()
	{
	}

	void AVPacketQueue::Unlock()
	{
		AutoLock autolock(m_mutex);
		m_stop = TRUE;
		m_cv.Unlock(FALSE);
	}

	BOOL AVPacketQueue::Push(INT32 index)
	{
		AVPacket pkt;
		av_init_packet(&pkt);
		pkt.data = NULL;
		pkt.size = 0;
		pkt.stream_index = index;
		return Push(pkt);
	}

	BOOL AVPacketQueue::Pop(AVPacket& pkt, INT32* serial)
	{
		AutoLock autolock(m_mutex);
		for (;;)
		{
			if (m_stop)
				return FALSE;

			if (m_linkqueue.size() > 0)
			{
				FF_AV_PACKET& pkt1 = m_linkqueue.front();
				m_size -= pkt1.pkt.size;
				m_duration -= pkt1.pkt.duration;
				pkt = pkt1.pkt;
				if (serial)
					*serial = pkt1.serial;
				m_linkqueue.pop_front();
				return TRUE;
			}
			else
			{
				m_cv.Lock(m_mutex);
			}
		}
		return TRUE;
	}

	int AVPacketQueue::GetBufferedAudioPacketDurationMS()
	{
		int          durationMS = 0;
		AutoLock autolock(m_mutex);

		if (m_linkqueue.size() == 0)
		{
			return 0;
		}
		else if (m_linkqueue.size() == 1)
		{
			return 0; // before decode,we can't calculate audio duration
		}
		else
		{
			FF_AV_PACKET& firstTSPacket = m_linkqueue.front();
			FF_AV_PACKET& lastTSPacket = m_linkqueue.back();
			durationMS = lastTSPacket.pkt.pts - firstTSPacket.pkt.pts;
			return durationMS;
		}
	}

	int AVPacketQueue::PacketCount()
	{
		return m_linkqueue.size();
	}

	BOOL AVPacketQueue::Push(AVPacket& pkt)
	{
		AutoLock autolock(m_mutex);
		if (m_stop)
			goto _FINISH;
		if (&pkt == &g_packet)
			m_serial++;
		FF_AV_PACKET value;
		value.pkt = pkt;
		value.serial = m_serial;
		m_size += value.pkt.size;
		m_duration += value.pkt.duration;
		m_linkqueue.push_back(value);
		m_cv.Unlock(FALSE);
		return TRUE;
	_FINISH:
		if (&pkt != &g_packet)
			av_packet_unref(&pkt);
		return FALSE;
	}

	void AVPacketQueue::Flush()
	{
		AutoLock autolock(m_mutex);
		while (!m_linkqueue.empty())
		{
			FF_AV_PACKET& value = m_linkqueue.front();
			av_packet_unref(&value.pkt);
			m_linkqueue.pop_front();
		}
		m_size = 0;
		m_duration = 0;
	}

	//////////////////////////////////////////////////////////////////////////
	AVFrameQueue::AVFrameQueue()
		: m_indexR(0)
		, m_indexW(0)
		, m_size(0)
		, m_maxsize(0)
		, m_indexshow(0)
	{
		ZeroMemory(m_frames, sizeof(FF_AV_FRAME) * FRAME_QUEUE_SIZE);
	}

	AVFrameQueue::~AVFrameQueue()
	{
	}

	INT32 AVFrameQueue::remaining() const
	{
		return m_size - m_indexshow;
	}

	FF_AV_FRAME* AVFrameQueue::PeekNext()
	{
		return &m_frames[(m_indexR + m_indexshow + 1) % m_maxsize];
	}

	FF_AV_FRAME* AVFrameQueue::PeakLast()
	{
		return &m_frames[m_indexR];
	}

	FF_AV_FRAME* AVFrameQueue::Peek(BOOL bREAD)
	{
		AutoLock autolock(m_mutex);
		if (!bREAD)
		{
			while (m_size >= m_maxsize && !m_packets.m_stop)
				m_cv.Lock(m_mutex);
			if (m_packets.m_stop)
				return NULL;
			return &m_frames[m_indexW];
		}
		else
		{
			while (m_size - m_indexshow <= 0 && !m_packets.m_stop)
				m_cv.Lock(m_mutex);
			if (m_packets.m_stop)
				return NULL;
			return &m_frames[(m_indexR + m_indexshow) % m_maxsize];
		}
	}

	void AVFrameQueue::Push()
	{
		if (++m_indexW == m_maxsize)
			m_indexW = 0;
		AutoLock autolock(m_mutex);
		m_size++;
		m_cv.Unlock(FALSE);
	}

	BOOL AVFrameQueue::Next()
	{
		if (!m_indexshow)
		{
			m_indexshow = 1;
			return FALSE;
		}
		av_frame_unref(m_frames[m_indexR].frame);
		if (++m_indexR == m_maxsize)
			m_indexR = 0;
		AutoLock autolock(m_mutex);
		m_size--;
		m_cv.Unlock(FALSE);
		return TRUE;
	}

	INT64 AVFrameQueue::GetLastPos()
	{
		FF_AV_FRAME& value = m_frames[m_indexR];
		if (m_indexshow && value.serial == m_packets.m_serial)
			return value.pos;
		return -1;
	}

	void AVFrameQueue::Unlock()
	{
		AutoLock autolock(m_mutex);
		m_cv.Unlock(FALSE);
	}

	BOOL AVFrameQueue::Create(INT32 maxsize)
	{
		Destroy();
		m_packets.m_stop = TRUE;
		m_maxsize = FFMIN(maxsize, FRAME_QUEUE_SIZE);
		for (INT32 i = 0; i < m_maxsize; i++)
		{
			m_frames[i].frame = av_frame_alloc();
			if (!m_frames[i].frame)
				goto _ERROR;
		}
		return TRUE;
	_ERROR:
		Destroy();
		return FALSE;
	}

	void AVFrameQueue::Destroy()
	{
		for (INT32 i = 0; i < m_maxsize; i++)
		{
			FF_AV_FRAME& value = m_frames[i];
			if (value.frame != NULL)
			{
				av_frame_unref(value.frame);
				av_frame_free(&value.frame);
			}
		}
		m_size = 0;
		m_indexR = 0;
		m_indexW = 0;
		m_maxsize = 0;
		m_indexshow = 0;
	}

	//////////////////////////////////////////////////////////////////////////
	Clock::Clock()
		: speed(1.0)
		, pts_drift(0.0)
		, last_updateds(0.0)
		, pts(0)
		, paused(FALSE)
		, queueserial(NULL)
	{
	}

	void Clock::SetupClock(INT32* queueserial)
	{
		this->speed = 1.0;
		this->paused = FALSE;
		this->queueserial = queueserial;
		SetClock(NAN, -1);
	}

	Clock::~Clock()
	{
	}

	DOUBLE Clock::GetClock()
	{
		if (!queueserial)
			return 0;
		if (*queueserial != serial)
			return NAN;
		if (this->paused)
		{
			return this->pts;
		}
		else
		{
			double time = av_gettime_relative() / 1000000.0;
			return this->pts_drift + time - (time - this->last_updateds) * (1.0 - this->speed);
		}
	}

	void Clock::SetClock(double pts, int serial)
	{
		double time = av_gettime_relative() / 1000000.0;
		SetClockAt(pts, serial, time);
	}

	void Clock::SetClockAt(double pts, int serial, double time)
	{
		this->pts = pts;
		this->last_updateds = time;
		this->pts_drift = this->pts - time;
		this->serial = serial;
	}

	void Clock::SetClockSpeed(double speed)
	{
		SetClock(GetClock(), serial);
		this->speed = speed;
	}
	//////////////////////////////////////////////////////////////////////////
	FF_AV::FF_AV()
		: m_finished(0)
		, m_serial(0)
		, m_bPending(FALSE)
		, m_bEnableHW(FALSE)
		, m_pts(0)
		, m_context(NULL)
		, m_contextHW(NULL)
		, m_continueCV(NULL)
		, m_index(-1)
		, m_stream(NULL)
	{
		ZeroMemory(&m_pkt, sizeof(m_pkt));
		ZeroMemory(&m_rational, sizeof(m_rational));
	}

	FF_AV::~FF_AV()
	{
	}

	AVStream* FF_AV::stream()
	{
		return m_stream;
	}

	BOOL FF_AV::Open(AVFormatContext* context, INT32 index, BOOL& bEnableHW)
	{
		INT32          iRes = 0;
		const AVCodec* codec = NULL;
		{
			m_context = avcodec_alloc_context3(NULL);
			if (!m_context)
			{
				goto _ERROR;
			}
			iRes = avcodec_parameters_to_context(m_context, context->streams[index]->codecpar);
			if (iRes < 0)
			{
				goto _ERROR;
			}
			m_context->pkt_timebase = context->streams[index]->time_base;
			if ((m_context->codec_id == AV_CODEC_ID_VP8) || (m_context->codec_id == AV_CODEC_ID_VP9))
			{
				AVDictionaryEntry* tag = NULL;
				tag = av_dict_get(context->streams[index]->metadata, "alpha_mode", tag, 0);
				if (tag && (strcmp(tag->value, "1") == 0))
				{
					const char* codec_name = (m_context->codec_id == AV_CODEC_ID_VP8) ? "libvpx" : "libvpx-vp9";
					codec = avcodec_find_decoder_by_name(codec_name);
				}
			}
			if (codec == NULL)
			{
				codec = avcodec_find_decoder(m_context->codec_id);
			}
			if (!codec)
			{
				goto _ERROR;
			}
			m_context->codec_id = codec->id;
			m_context->flags2 |= AV_CODEC_FLAG2_FAST;
			AVDictionary* opts = NULL;
			if (!av_dict_get(opts, "threads", NULL, 0))
				av_dict_set(&opts, "threads", "auto", 0);
			av_dict_set(&opts, "refcounted_frames", "1", 0);
			iRes = avcodec_open2(m_context, codec, &opts);
			if (iRes < 0)
			{
				goto _ERROR;
			}
			context->streams[index]->discard = AVDISCARD_DEFAULT;
			m_stream = context->streams[index];
			m_index = index;
			m_queue.m_packets.m_stop = FALSE;
			m_queue.m_packets.Push(g_packet);
		}
		return TRUE;
	_ERROR:
		Close();
		return FALSE;
	}

	void FF_AV::Close()
	{
		Stop();
		if (m_task.joinable())
			m_task.join();
		m_queue.m_packets.Flush();
		m_queue.Destroy();
		av_packet_unref(&m_pkt);
		if (m_context != NULL)
			avcodec_free_context(&m_context);
		m_context = NULL;
		if (m_contextHW != NULL)
			av_buffer_unref(&m_contextHW);
		m_contextHW = NULL;
		m_index = -1;
		m_stream = NULL;
	}

	void FF_AV::Stop()
	{
		m_bPending = FALSE;
		m_queue.m_packets.Unlock();
		m_queue.Unlock();
	}

	void FF_AV::Unlock()
	{
		m_bPending = FALSE;
		m_queue.m_packets.Unlock();
		m_queue.Unlock();
	}

	//////////////////////////////////////////////////////////////////////////
	static INT32 decode_interrupt_cb(void* ctx)
	{
		AVDemuxer* pThis = (AVDemuxer*)ctx;
		return pThis->IsExiting();
	}

	//////////////////////////////////////////////////////////////////////////
	AVDemuxer::AVDemuxer()
		: m_context(NULL)
		, m_exiting(FALSE)
		, m_bEOF(FALSE)
		, m_bStep(FALSE)
		, m_bEnableLoop(FALSE)
		, m_bEnableHW(FALSE)
		, m_bPause(FALSE)
		, m_bPreviousPause(FALSE)
		, m_bAttachment(FALSE)
		, m_bPresent(FALSE)
		, m_timestamp(0)
		, m_seekPos(0)
		, m_seekRel(0)
		, m_seekFlag(0)
		, m_beginTS(0)
		, m_maxduration(0.0)
		, m_previousPos(0.0)
		, m_videoPTS(0.0)
		, m_audioPTS(NAN)
		, m_lasttime(0)
		, m_lastdelay(0)
		, m_buffersize(1024 * 1024 * 3)
		, m_reconnects(3)
		, m_swscontext(NULL)
		, m_graph(NULL)
		, m_bufferFilter(NULL)
		, m_buffersinkFilter(NULL)
		, m_yadifFilter(NULL)
		, m_bestFMT(AV_PIX_FMT_NONE)
	{

	}

	AVDemuxer::~AVDemuxer()
	{
	}

	BOOL AVDemuxer::IsExiting() const
	{
		return m_exiting;
	}

	BOOL AVDemuxer::Open(const std::string& szNAME)
	{
		m_szNAME = szNAME;
		m_bEOF = FALSE;
		m_bStep = FALSE;
		m_bPause = FALSE;
		m_bPreviousPause = FALSE;
		m_bAttachment = FALSE;
		m_bPresent = FALSE;
		m_bestFMT = AV_PIX_FMT_NONE;
		av_init_packet(&g_packet);
		g_packet.data = (UINT8*)&g_packet;
		if (!m_audio.m_queue.Create(AUDIO_QUEUE_SIZE))
		{
			goto _ERROR;
		}
		if (!m_video.m_queue.Create(VIDEO_QUEUE_SIZE))
		{
			goto _ERROR;
		}
		m_audioclock.SetupClock(&(m_audio.m_queue.m_packets.m_serial));
		m_videoclock.SetupClock(&(m_video.m_queue.m_packets.m_serial));
		m_readTask = std::thread(&AVDemuxer::OnReadPump, this);
		return TRUE;
	_ERROR:
		Close();
		return FALSE;
	}

	void AVDemuxer::UninitializeFilter()
	{
		if (m_graph != NULL)
			avfilter_graph_free(&m_graph);
		m_graph = NULL;
		m_bufferFilter = NULL;
		m_buffersinkFilter = NULL;
		m_yadifFilter = NULL;
	}

	BOOL AVDemuxer::InitializeFilter(AVFrame* video)
	{
		INT32 iRes = 0;
		CHAR       bufferargs[256];
		AVRational tb = av_guess_frame_rate(m_context, m_video.m_stream, NULL);
		UninitializeFilter();
		m_graph = avfilter_graph_alloc();
		if (!m_graph)
		{
			iRes = AVERROR(ENOMEM);
			goto _ERROR;
		}
		snprintf(bufferargs, sizeof(bufferargs), "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
			video->width, video->height, video->format, m_video.stream()->time_base.num, m_video.stream()->time_base.den,
			m_video.stream()->codecpar->sample_aspect_ratio.num,
			FFMAX(m_video.stream()->codecpar->sample_aspect_ratio.den, 1));
		if (tb.num && tb.den)
			av_strlcatf(bufferargs, sizeof(bufferargs), ":frame_rate=%d/%d", tb.num, tb.den);
		iRes = avfilter_graph_create_filter(&m_bufferFilter, avfilter_get_by_name("buffer"), "FFDemuxer_buffer", bufferargs,
			nullptr, m_graph);
		if (iRes < 0)
		{
			goto _ERROR;
		}
		iRes = avfilter_graph_create_filter(&m_buffersinkFilter, avfilter_get_by_name("buffersink"), "FFDemuxer_buffersink",
			nullptr, nullptr, m_graph);
		if (iRes < 0)
		{
			goto _ERROR;
		}
		iRes = avfilter_graph_create_filter(&m_yadifFilter, avfilter_get_by_name("yadif"), "FFDemuxer_yadif", nullptr,
			nullptr, m_graph);
		if (iRes < 0)
		{
			goto _ERROR;
		}
		iRes = avfilter_link(m_bufferFilter, 0, m_yadifFilter, 0);
		if (iRes < 0)
		{
			goto _ERROR;
		}
		iRes = avfilter_link(m_yadifFilter, 0, m_buffersinkFilter, 0);
		if (iRes < 0)
		{
			goto _ERROR;
		}
		iRes = avfilter_graph_config(m_graph, NULL);
		if (iRes < 0)
		{
			goto _ERROR;
		}
		return TRUE;
	_ERROR:
		UninitializeFilter();
		return FALSE;
	}

	void AVDemuxer::Close()
	{
		m_exiting = TRUE;
		m_audio.Stop();
		m_video.Stop();
		if (m_readTask.joinable())
			m_readTask.join();
		if (m_audioTask.joinable())
			m_audioTask.join();
		if (m_audio.m_task.joinable())
			m_audio.m_task.join();
		if (m_videoTask.joinable())
			m_videoTask.join();
		if (m_video.m_task.joinable())
			m_video.m_task.join();

		m_video.Close();
		m_audio.Close();

		m_resampler.Close();

		m_bEOF = FALSE;
		m_seekFlag = 0;
		m_bStep = FALSE;
		m_bPause = FALSE;
		m_bPreviousPause = FALSE;
		m_bAttachment = FALSE;
		m_bPresent = FALSE;
		m_exiting = FALSE;
	}

	void AVDemuxer::OnReadPump()
	{
		if (m_exiting)
		{
			return;
		}
		BOOL     bRes = FALSE;
		INT32    iRes = 0;
		AVPacket pkt;
		av_init_packet(&pkt);
		AVDictionary* opts = NULL;
		av_dict_set(&opts, "stimeout", "30000000", 0);
		if (!m_buffersize)
			av_dict_set(&opts, "fflags", "nobuffer", 0);
		else
			av_dict_set_int(&opts, "buffer_size", m_buffersize, 0);
		if (!av_dict_get(opts, "threads", NULL, 0))
			av_dict_set(&opts, "threads", "auto", 0);
		m_context = avformat_alloc_context();
		if (!m_context)
		{
			goto _ERROR;
		}
		if (!m_buffersize)
			m_context->flags |= AVFMT_FLAG_NOBUFFER;
		m_context->interrupt_callback.callback = decode_interrupt_cb;
		m_context->interrupt_callback.opaque = this;
		iRes = avformat_open_input(&m_context, m_szNAME.c_str(), NULL, &opts);
		if (iRes < 0)
		{
			goto _ERROR;
		}
		if (m_context->pb && (!strncmp(m_context->url, "http:", 5)))
			av_dict_set(&opts, "timeout", "5000", 0);
		av_format_inject_global_side_data(m_context);
		iRes = avformat_find_stream_info(m_context, NULL);
		if (iRes < 0)
		{
			goto _ERROR;
		}
		if (m_context->pb != NULL)
			m_context->pb->eof_reached = 0;
		m_maxduration = (m_context->iformat->flags & AVFMT_TS_DISCONT) ? 10.0 : 3600.0;
		INT32 indexs[AVMEDIA_TYPE_NB];
		memset(indexs, -1, sizeof(indexs));
		for (INT32 i = 0; i < m_context->nb_streams; i++)
		{
			AVStream*        stream = m_context->streams[i];
			enum AVMediaType type = (enum AVMediaType)stream->codecpar->codec_type;
			stream->discard = AVDISCARD_ALL;
			if (type >= 0 && g_wanted_stream_spec[type] && indexs[type] == -1)
				if (avformat_match_stream_specifier(m_context, stream, g_wanted_stream_spec[type]) > 0)
					indexs[type] = i;
		}
		for (INT32 i = 0; i < AVMEDIA_TYPE_NB; i++)
		{
			if (g_wanted_stream_spec[i] && indexs[i] == -1)
			{
				indexs[i] = INT_MAX;
			}
		}
		indexs[AVMEDIA_TYPE_AUDIO] = av_find_best_stream(m_context, AVMEDIA_TYPE_AUDIO, indexs[AVMEDIA_TYPE_AUDIO],
			indexs[AVMEDIA_TYPE_VIDEO], NULL, 0);
		indexs[AVMEDIA_TYPE_VIDEO] =
			av_find_best_stream(m_context, AVMEDIA_TYPE_VIDEO, indexs[AVMEDIA_TYPE_VIDEO], -1, NULL, 0);
		if (indexs[AVMEDIA_TYPE_AUDIO] >= 0)
		{
			BOOL bEnableHW = FALSE;
			if (!m_audio.Open(m_context, indexs[AVMEDIA_TYPE_AUDIO], bEnableHW))
			{
				goto _ERROR;
			}
			m_audio.m_serial = -1;
			m_audio.m_continueCV = &m_continueCV;
			AVCodecContext* context = m_audio.m_context;
			INT32           channels = context->channels;
			UINT64          channellayout = context->channel_layout;
			if (!channellayout || channels != av_get_channel_layout_nb_channels(channellayout))
			{
				channellayout = av_get_default_channel_layout(channels);
				channellayout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX;
			}
			m_audioParameterO.SetChannels(2);
			m_audioParameterO.SetChannelLayout(IPC_CHANNEL_STEREO);
			m_audioParameterO.SetSamplesPerSec(context->sample_rate);
			m_audioParameterO.SetAudioFormat(IPC_AUDIO_FORMAT_16BIT);
			m_audioParameterO.SetBitsPerSample(16);
			if (AUDIO_START != nullptr)
				AUDIO_START(m_audioParameterO);
			m_audioTask = std::thread(&AVDemuxer::OnAudioRenderPump, this);
			m_audio.m_task = std::thread(&AVDemuxer::OnAudioDecodePump, this);
		}
		if (indexs[AVMEDIA_TYPE_VIDEO] >= 0)
		{
			if (!m_video.Open(m_context, indexs[AVMEDIA_TYPE_VIDEO], m_bEnableHW))
				goto _ERROR;
			if (m_video.m_context->codec_id == AV_CODEC_ID_RAWVIDEO)
			{
				goto _ERROR;
			}
			m_video.m_serial = -1;
			m_video.m_continueCV = &m_continueCV;
			AVCodecContext* context = m_video.m_context;
			m_videoParameter.SetCS(MS_AVCOL_SPC_TO_COLOR_SPACE(context->colorspace));
			m_videoParameter.SetCT(MS_AVCOL_SPC_TO_COLOR_TRANSFER(context->color_trc));
			m_videoParameter.SetRange(context->color_range == AVCOL_RANGE_JPEG ? IPC_VIDEO_RANGE_FULL : IPC_VIDEO_RANGE_PARTIAL);
			m_videoParameter.SetSize({ context->width, context->height });
			m_videoParameter.SetFormat(MS_AV_PIX_FMT_TO_VIDEO_PIXEL_FORMAT(context->pix_fmt));
			m_video.m_task = std::thread(&AVDemuxer::OnVideoDecodePump, this);
			m_videoTask = std::thread(&AVDemuxer::OnVideoRenderPump, this);
			m_bAttachment = TRUE;
		}
		for (;;)
		{
			if (m_exiting)
				break;
			if (m_bPause != m_bPreviousPause)
			{
				m_bPreviousPause = m_bPause;
				if (m_bPause)
					av_read_pause(m_context);
				else
					av_read_play(m_context);
			}
			if (m_bPause &&
				(!strcmp(m_context->iformat->name, "rtsp") || (m_context->pb && !strncmp(m_szNAME.c_str(), "mmsh:", 5))))
			{
				Sleep(10);
				continue;
			}
			if (m_bAttachment)
			{
				if (m_video.m_stream != NULL && m_video.m_stream->disposition & AV_DISPOSITION_ATTACHED_PIC)
				{
					AVPacket copy;
					if ((iRes = av_packet_ref(&copy, &m_video.m_stream->attached_pic)) < 0)
					{
						bRes = TRUE;
						goto _ERROR;
					}
					m_video.m_queue.m_packets.Push(copy);
					m_video.m_queue.m_packets.Push(m_video.m_index);
				}
				m_bAttachment = FALSE;
			}
			if (((m_audio.m_queue.m_packets.m_size + m_video.m_queue.m_packets.m_size) > MAX_QUEUE_SIZE ||
				(CheckPackets(m_audio.m_stream, m_audio.m_index, &m_audio.m_queue.m_packets) &&
					CheckPackets(m_video.m_stream, m_video.m_index, &m_video.m_queue.m_packets))))
			{
				AutoLock autolock(m_mutexCV);
				m_continueCV.TryLock(m_mutexCV, 10);
				continue;
			}
			iRes = av_read_frame(m_context, &pkt);
			if (iRes < 0)
			{
				if ((iRes == AVERROR_EOF || avio_feof(m_context->pb)) && !m_bEOF)
				{
					if (m_video.m_index >= 0)
						m_video.m_queue.m_packets.Push(m_video.m_index);
					if (m_audio.m_index >= 0)
						m_audio.m_queue.m_packets.Push(m_audio.m_index);
					m_bEOF = TRUE;
				}
				if (m_context->pb != NULL && m_context->pb->error != NULL)
				{
					bRes = TRUE;
					break;
				}
				AutoLock autolock(m_mutexCV);
				m_continueCV.TryLock(m_mutexCV, 10);
				continue;
			}
			else
			{
				m_bEOF = FALSE;
			}
			if (pkt.stream_index == m_audio.m_index)
			{
				m_audio.m_queue.m_packets.Push(pkt);
			}
			else if (pkt.stream_index == m_video.m_index && !(m_video.m_stream->disposition & AV_DISPOSITION_ATTACHED_PIC))
			{
				m_video.m_queue.m_packets.Push(pkt);

			}
			else
			{
				av_packet_unref(&pkt);
			}
		}
	_ERROR:
		if (opts != NULL)
			av_dict_free(&opts);
		opts = NULL;
		BOOL already_in = m_exiting.exchange(TRUE);
		m_audio.Stop();
		if (m_audioTask.joinable())
			m_audioTask.join();
		if (m_audio.m_task.joinable())
			m_audio.m_task.join();
		m_video.Stop();
		if (m_videoTask.joinable())
			m_videoTask.join();
		if (m_video.m_task.joinable())
			m_video.m_task.join();
		if (m_context != NULL)
			avformat_close_input(&m_context);
		m_context = NULL;
		if (m_swscontext != NULL)
		{
			sws_freeContext(m_swscontext);
			av_freep(&m_pointers[0]);
		}
		m_swscontext = NULL;
		UninitializeFilter();
	}

	void AVDemuxer::OnAudioDecodePump()
	{
		BOOL         bRes = FALSE;
		INT32        got = 0;
		FF_AV_FRAME* value = NULL;
		AVRational   tb;
		AVFrame*     audio = NULL;
		audio = av_frame_alloc();
		if (!audio)
		{
			goto _END;
		}
		for (;;)
		{
			if (m_exiting)
				goto _END;
			if ((got = OnDecodeFrame(m_audio, audio)) < 0)
			{
				bRes = TRUE;
				goto _END;
			}
			if (got)
			{
				tb = { 1, audio->sample_rate };
				value = m_audio.m_queue.Peek();
				if (!value)
					goto _END;
				value->pts = (audio->pts == AV_NOPTS_VALUE) ? NAN : audio->pts * av_q2d(tb);
				value->pos = audio->pkt_pos;
				value->serial = m_audio.m_serial;
				value->duration = av_q2d({ audio->nb_samples, audio->sample_rate });
				av_frame_move_ref(value->frame, audio);
				m_audio.m_queue.Push();
			}
		}
	_END:
		av_frame_free(&audio);
	}

	double AVDemuxer::CalculateDuration(FF_AV_FRAME* vp, FF_AV_FRAME* nextvp)
	{
		if (vp->serial == nextvp->serial)
		{
			double duration = nextvp->pts - vp->pts;
			if (isnan(duration) || duration <= 0 || duration > m_maxduration)
				return vp->duration;
			else
				return duration;
		}
		else
		{
			return 0.0;
		}
	}

	double AVDemuxer::CalculateDelay(double delay)
	{
		double sync_threshold, diff = 0;
		diff = m_videoclock.GetClock() - m_audioclock.GetClock();
		sync_threshold = FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(AV_SYNC_THRESHOLD_MAX, delay));
		if (!isnan(diff) && fabs(diff) < m_maxduration)
		{
			if (diff <= -sync_threshold)
				delay = FFMAX(0, delay + diff);
			else if (diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD)
				delay = delay + diff;
			else if (diff >= sync_threshold)
				delay = 2 * delay;
		}
		return delay;
	}

	BOOL AVDemuxer::OnVideoRender(double* remaining)
	{
		if (!m_video.m_stream)
			return FALSE;
		double time = 0;
	_RETRY:
		if (m_video.m_queue.remaining() != 0)
		{
			double       lastduration, duration, delay = 0;
			FF_AV_FRAME* currentvp = NULL;
			FF_AV_FRAME* lastvp = NULL;
			lastvp = m_video.m_queue.PeakLast();
			currentvp = m_video.m_queue.Peek(TRUE);
			if (!currentvp || !lastvp)
				return FALSE;
			if (currentvp->serial != m_video.m_queue.m_packets.m_serial)
			{
				m_video.m_queue.Next();
				goto _RETRY;
			}
			if (lastvp->serial != currentvp->serial)
				m_videoPTS = av_gettime_relative() / 1000000.0;
			if (m_bPause)
				goto _DISPLAY;
			lastduration = CalculateDuration(lastvp, currentvp);
			delay = CalculateDelay(lastduration);
			time = av_gettime_relative() / 1000000.0;
			if (time < m_videoPTS + delay)
			{
				*remaining = FFMIN(m_videoPTS + delay - time, *remaining);
				goto _DISPLAY;
			}
			m_videoPTS += delay;
			if (delay > 0 && time - m_videoPTS > AV_SYNC_THRESHOLD_MAX)
				m_videoPTS = time;
			m_video.m_queue.m_mutex.Lock();
			if (!isnan(currentvp->pts))
			{
				m_videoclock.SetClock(currentvp->pts, currentvp->serial);
				m_previousPos = m_videoclock.GetClock();
			}
			m_video.m_queue.m_mutex.Unlock();
			if (m_video.m_queue.remaining() > 1)
			{
				FF_AV_FRAME* nextvp = m_video.m_queue.PeekNext();
				duration = CalculateDuration(currentvp, nextvp);
				if (!m_bStep && time > (m_videoPTS + duration))
				{
					m_video.m_queue.Next();
					goto _RETRY;
				}
			}
			m_video.m_queue.Next();
			m_bPresent = TRUE;
			if (m_bStep && !m_bPause)
			{
				m_bPause = m_videoclock.paused = m_audioclock.paused = !m_bPause;
			}
		}
	_DISPLAY:
		if (m_video.m_queue.m_indexshow && m_bPresent)
		{
			FF_AV_FRAME* vp = NULL;
			vp = m_video.m_queue.PeakLast();
			if (vp != NULL && vp->frame != NULL)
			{
				INT32              iRes = 0;
				enum AVPixelFormat format = (enum AVPixelFormat)vp->frame->format;
				enum AVPixelFormat bestFMT = AV_PIX_FMT_RGBA;
				if (bestFMT != format)
				{
					if (m_bestFMT != bestFMT)
					{
						if (m_swscontext != NULL)
						{
							sws_freeContext(m_swscontext);
							av_freep(&m_pointers[0]);
						}
						m_swscontext = NULL;
						INT32      space = MS_AV_CS_TO_SWS_CS(vp->frame->colorspace);
						INT32      range = vp->frame->color_range == AVCOL_RANGE_JPEG ? 1 : 0;
						const int* coeff = sws_getCoefficients(space);
						m_swscontext = sws_getCachedContext(NULL, vp->frame->width, vp->frame->height, format, vp->frame->width,
							vp->frame->height, bestFMT, SWS_POINT, NULL, NULL, NULL);
						if (!m_swscontext)
						{
							return FALSE;
						}
						iRes = av_image_alloc(m_pointers, m_linesizes, m_video.m_context->width, m_video.m_context->height, bestFMT, 32);
						if (iRes < 0)
						{
							return FALSE;
						}
#define FIXED_1_0 (1 << 16)
						sws_setColorspaceDetails(m_swscontext, coeff, range, coeff, range, 0, FIXED_1_0, FIXED_1_0);
						m_bestFMT = bestFMT;
					}
				}
				IPC_VIDEO_PIXEL_FORMAT vFORMAT = MS_AV_PIX_FMT_TO_VIDEO_PIXEL_FORMAT(bestFMT);
				m_videoParameter.SetFormat(vFORMAT);
				IPC_PACKET vpk;
				ZeroMemory(&vpk, sizeof(vpk));
				if (m_swscontext != NULL)
				{
					iRes = sws_scale(m_swscontext, (const UINT8* const*)vp->frame->data, vp->frame->linesize, 0, vp->frame->height,
						m_pointers, m_linesizes);
					if (iRes < 0)
					{
						return FALSE;
					}
					for (UINT32 i = 0; i < 4; i++)
					{
						vpk.data[i] = m_pointers[i];
						vpk.linesize[i] = abs(m_linesizes[i]);
					}
				}
				else
				{
					for (UINT32 i = 0; i < 4; i++)
					{
						vpk.data[i] = vp->frame->data[i];
						vpk.linesize[i] = abs(vp->frame->linesize[i]);
					}
				}

				vpk.type = IPC_VIDEO;
				vpk.timestamp = PipeSDK::GetTimeNS();
				vpk.video.cx = vp->frame->width;
				vpk.video.cy = vp->frame->height;
				vpk.video.format = vFORMAT;
				vpk.video.cs = MS_AVCOL_SPC_TO_COLOR_SPACE(vp->frame->colorspace);
				vpk.video.ct = MS_AVCOL_SPC_TO_COLOR_TRANSFER(vp->frame->color_trc);
				vpk.video.range = vp->frame->color_range == AVCOL_RANGE_JPEG ? IPC_VIDEO_RANGE_FULL : IPC_VIDEO_RANGE_PARTIAL;
				vpk.video.angle = 0;
				vpk.video.bFlipH = FALSE;
				vpk.video.bFlipV = FALSE;
				if (VIDEO_PACKET != nullptr)
					VIDEO_PACKET(vpk);

			}
		}
		m_bPresent = FALSE;
		return TRUE;
	}

	void AVDemuxer::OnVideoRenderPump()
	{
		if (!m_video.m_stream)
			return;
		double remaining = 0.0;
		for (;;)
		{
			if (m_exiting)
				break;
			if (remaining > 0.0)
				av_usleep((int64_t)(remaining * 1000000.0));
			remaining = REFRESH_RATE;
			if (!m_bPause || m_bPresent)
			{
				OnVideoRender(&remaining);
			}
		}
	}

	void AVDemuxer::OnVideoDecodePump()
	{
		BOOL     bRes = FALSE;
		INT32    iRes = 0;
		INT32    lastcx = 0;
		INT32    lastcy = 0;
		INT32    lastformat = 0;
		INT32    lastserial = 0;
		DOUBLE   pts = 0;
		DOUBLE   duration = 0;
		AVFrame* video = NULL;
		video = av_frame_alloc();
		if (!video)
			return;
		AVRational tb = m_video.m_stream->time_base;
		AVRational tb2 = av_guess_frame_rate(m_context, m_video.m_stream, NULL);
		for (;;)
		{
			if (m_exiting)
				goto _END;
			iRes = GetVideoFrame(video);
			if (iRes < 0)
			{
				bRes = TRUE;
				goto _END;
			}
			if (!iRes)
				continue;
			if (video->interlaced_frame)
			{
				if (lastcx != video->width || lastcy != video->height || lastformat != video->format ||
					lastserial != m_video.m_serial)
				{
					if (!InitializeFilter(video))
						goto _END;
					lastcx = video->width;
					lastcy = video->height;
					lastformat = video->format;
					lastserial = m_video.m_serial;
				}
				iRes = av_buffersrc_add_frame(m_bufferFilter, video);
				if (iRes < 0)
				{
					goto _END;
				}
				while (iRes >= 0)
				{
					m_lasttime = av_gettime_relative() / 1000000.0;
					iRes = av_buffersink_get_frame_flags(m_buffersinkFilter, video, 0);
					if (iRes < 0)
					{
						if (iRes == AVERROR_EOF)
							m_video.m_finished = m_video.m_serial;
						iRes = 0;
						break;
					}
					m_lastdelay = av_gettime_relative() / 1000000.0 - m_lasttime;
					if (fabs(m_lastdelay) > AV_NOSYNC_THRESHOLD / 10.0)
						m_lastdelay = 0;
					tb = av_buffersink_get_time_base(m_buffersinkFilter);
					duration = (tb2.num && tb2.den ? av_q2d({ tb2.den, tb2.num }) : 0);
					pts = (video->pts == AV_NOPTS_VALUE) ? NAN : video->pts * av_q2d(tb);
					iRes = OnQueuePicture(video, pts, duration, video->pkt_pos, m_video.m_serial);
					if (video != NULL)
						av_frame_unref(video);
					if (m_video.m_queue.m_packets.m_serial != m_video.m_serial)
						break;
				}
			}
			else
			{
				duration = (tb2.num && tb2.den ? av_q2d({ tb2.den, tb2.num }) : 0);
				pts = (video->pts == AV_NOPTS_VALUE) ? NAN : video->pts * av_q2d(tb);
				iRes = OnQueuePicture(video, pts, duration, video->pkt_pos, m_video.m_serial);
				if (video != NULL)
					av_frame_unref(video);
			}
			if (iRes < 0)
				goto _END;
		}
	_END:
		if (m_graph != NULL)
			avfilter_graph_free(&m_graph);
		m_graph = NULL;
		if (video != NULL)
			av_frame_unref(video);
		av_frame_free(&video);
	}
	inline UINT32 CalculateSamples(UINT32 samplesPerSec)
	{
		return (std::max)(1024, 2 << av_log2(samplesPerSec / 30));
	}
	void AVDemuxer::OnAudioRenderPump()
	{
		INT32  serial = 0;
		UINT32 avgBytesPerSec = m_audioParameterO.GetBlockSize() * m_audioParameterO.GetSamplesPerSec();
		INT32  chunksize = CalculateSamples(m_audio.m_context->sample_rate) *
			((m_audioParameterO.GetChannels() * m_audioParameterO.GetBitsPerSample()) / 8);
		DOUBLE            audioPTS = NAN;
		std::vector<UINT8> buffer;
		buffer.resize(chunksize);
		for (;;)
		{
			if (m_exiting)
				break;;
			m_timestamp = av_gettime_relative();
			UINT8* output[MAX_AV_PLANES];
			ZeroMemory(output, sizeof(output));
			INT32 iRes = GetAudioFrame(output, serial, audioPTS, chunksize);
			if (iRes > 0)
			{
				if (m_aq.size() <= MAX_AUDIO_BUFFER_SIZE)
				{
					m_aq.Push(output[0], iRes);
					const UINT8* data = NULL;
					INT32 size = 0;
					m_aq.Peek(&data, &size);
					if (size >= chunksize)
					{
						memcpy(buffer.data(), data, chunksize);
						m_aq.Pop(chunksize);
						PipeSDK::IPC_PACKET pkt;
						ZeroMemory(&pkt, sizeof(pkt));
						pkt.type = IPC_AUDIO;
						pkt.audio.blocksize = static_cast<UINT32>(m_audioParameterO.GetChannels() * (m_audioParameterO.GetBitsPerSample() / 8));
						pkt.audio.count = chunksize / pkt.audio.blocksize;
						pkt.audio.sampleRate = m_audioParameterO.GetSamplesPerSec();
						pkt.audio.layout = m_audioParameterO.GetChannelLayout();
						pkt.audio.format = m_audioParameterO.GetAudioFormat();
						pkt.audio.planes = m_audioParameterO.GetPlanes();
						pkt.timestamp = PipeSDK::GetTimeNS();
						pkt.size = pkt.audio.count * pkt.audio.blocksize;
						pkt.data[0] = (UINT8*)buffer.data();
						pkt.linesize[0] = buffer.size();
						if (AUDIO_PACKET != nullptr)
							AUDIO_PACKET(pkt);
						if (!isnan(audioPTS))
						{
							double diff = static_cast<DOUBLE>((double)audioPTS - static_cast<DOUBLE>((double)chunksize / (double)avgBytesPerSec));
							m_audioclock.SetClockAt(diff, serial, static_cast<DOUBLE>(static_cast<double>(m_timestamp) / 1000000.0));
						}
					}
					continue;
				}
				else
				{
					m_aq.Reset();
				}
			}
			else
				av_usleep(1000);
			buffer.resize(chunksize);
			memset(buffer.data(), 0, chunksize);
			PipeSDK::IPC_PACKET pkt;
			ZeroMemory(&pkt, sizeof(pkt));
			pkt.type = IPC_AUDIO;
			pkt.audio.blocksize = static_cast<UINT32>(m_audioParameterO.GetChannels() * (m_audioParameterO.GetBitsPerSample() / 8));
			pkt.audio.count = chunksize / pkt.audio.blocksize;
			pkt.audio.sampleRate = m_audioParameterO.GetSamplesPerSec();
			pkt.audio.layout = m_audioParameterO.GetChannelLayout();
			pkt.audio.format = m_audioParameterO.GetAudioFormat();
			pkt.audio.planes = m_audioParameterO.GetPlanes();
			pkt.timestamp = PipeSDK::GetTimeNS();
			pkt.size = pkt.audio.count * pkt.audio.blocksize;
			pkt.data[0] = (UINT8*)buffer.data();
			pkt.linesize[0] = buffer.size();
			if (AUDIO_PACKET != nullptr)
				AUDIO_PACKET(pkt);
		}
	}

	INT32 AVDemuxer::GetAudioFrame(UINT8* output[], INT32& serial, DOUBLE& audioPTS, INT32 chunksize)
	{
		if (m_bPause || m_bStep)
			return -1;
		BOOL         bRes = FALSE;
		FF_AV_FRAME* value = NULL;
		UINT32       avgBytesPerSec = m_audioParameterO.GetSamplesPerSec() * m_audioParameterO.GetBlockSize();
		do
		{
			while (m_audio.m_queue.remaining() == 0 && !m_exiting)
			{
				if ((av_gettime_relative() - m_timestamp) > (1000000LL * chunksize / avgBytesPerSec / 2))
					return -1;
				av_usleep(100);
			}
			value = m_audio.m_queue.Peek(TRUE);
			if (!value)
				return -1;
			m_audio.m_queue.Next();
		} while (value->serial != m_audio.m_queue.m_packets.m_serial);
		INT32          size = av_samples_get_buffer_size(NULL, value->frame->channels, value->frame->nb_samples,
			(enum AVSampleFormat)value->frame->format, 1);
		INT64          channellayout = (value->frame->channel_layout &&
			value->frame->channels == av_get_channel_layout_nb_channels(value->frame->channel_layout))
			? value->frame->channel_layout
			: av_get_default_channel_layout(value->frame->channels);
		AudioParameter cur;
		cur.SetAudioFormat(MS_AV_SAMPLE_FMT_TO_AUDIO_FORMAT((enum AVSampleFormat)value->frame->format));
		cur.SetBitsPerSample(MS_AV_SAMPLE_FMT_TO_SIZE((enum AVSampleFormat)value->frame->format));
		cur.SetChannelLayout(MS_AV_CH_LAYOUT_TO_CHANNEL_LAYOUT(channellayout));
		cur.SetSamplesPerSec(value->frame->sample_rate);
		cur.SetChannels(value->frame->channels);
		if (cur != m_audioParameterI)
		{
			m_resampler.Close();
			m_audioParameterI = cur;
		}

		if (m_audioParameterO != cur)
		{
			if (m_resampler.IsEmpty())
			{
				if (!m_resampler.Open(m_audioParameterI, m_audioParameterO))
				{
					return -1;
				}
			}
		}
		if (!m_resampler.IsEmpty())
		{
			UINT64        delay = 0;
			const UINT8** input = (const UINT8**)value->frame->extended_data;
			UINT32        countO = value->frame->nb_samples + 256;
			bRes = m_resampler.Resample(output, &countO, &delay, input, value->frame->nb_samples);
			if (!bRes)
			{
				return -1;
			}
			size = countO * m_audioParameterO.GetChannels() * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
		}
		else
		{
			output[0] = value->frame->data[0];
		}
		if (!isnan(value->pts))
			audioPTS = value->pts + (double)value->frame->nb_samples / value->frame->sample_rate;
		else
			audioPTS = NAN;
		serial = value->serial;
		return size;
	}

	INT32 AVDemuxer::GetVideoFrame(AVFrame* video)
	{
		INT32 got = 0;
		if ((got = OnDecodeFrame(m_video, video)) < 0)
			return -1;
		if (got)
		{
			DOUBLE dpts = NAN;
			if (video->pts != AV_NOPTS_VALUE)
				dpts = av_q2d(m_video.m_stream->time_base) * video->pts;
			video->sample_aspect_ratio = av_guess_sample_aspect_ratio(m_context, m_video.m_stream, video);
			if (video->pts != AV_NOPTS_VALUE)
			{
				DOUBLE clock = m_audioclock.GetClock();
				DOUBLE diff = dpts - clock;
				if (!isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD && ((diff - m_lastdelay) < 0) &&
					(m_video.m_serial == m_videoclock.serial) && m_video.m_queue.m_packets.m_linkqueue.size())
				{
					av_frame_unref(video);
					got = 0;
				}
			}
		}
		return got;
	}

	INT32 AVDemuxer::OnQueuePicture(AVFrame* video, DOUBLE pts, DOUBLE duration, INT64 pos, INT32 serial)
	{
		FF_AV_FRAME* value = NULL;
		value = m_video.m_queue.Peek();
		if (!value)
			return -1;
		value->rational = video->sample_aspect_ratio;
		value->width = video->width;
		value->height = video->height;
		value->format = video->format;
		value->pts = pts;
		value->duration = duration;
		value->pos = pos;
		value->serial = serial;
		av_frame_move_ref(value->frame, video);
		m_video.m_queue.Push();
		return 0;
	}

	INT32 AVDemuxer::OnDecodeFrame(FF_AV& av, AVFrame* frame)
	{
		INT32 iRes = AVERROR(EAGAIN);
		for (;;)
		{
			if (m_exiting)
				return -1;
			AVPacket pkt;
			if (av.m_queue.m_packets.m_serial == av.m_serial)
			{
				do
				{
					if (m_exiting)
						return -1;
					if (av.m_queue.m_packets.m_stop)
						return -1;
					switch (av.m_context->codec_type)
					{
					case AVMEDIA_TYPE_VIDEO:
					{
						iRes = avcodec_receive_frame(av.m_context, frame);
						if (iRes >= 0)
						{
							frame->pts = frame->best_effort_timestamp;
						}
					}
					break;
					case AVMEDIA_TYPE_AUDIO:
					{
						iRes = avcodec_receive_frame(av.m_context, frame);
						if (iRes >= 0)
						{
							AVRational tb = { 1, frame->sample_rate };
							if (frame->pts != AV_NOPTS_VALUE)
								frame->pts = av_rescale_q(frame->pts, av.m_context->pkt_timebase, tb);
							else if (av.m_pts != AV_NOPTS_VALUE)
								frame->pts = av_rescale_q(av.m_pts, av.m_rational, tb);
							if (frame->pts != AV_NOPTS_VALUE)
							{
								av.m_pts = frame->pts + frame->nb_samples;
								av.m_rational = tb;
							}
						}
					}
					break;
					}
					if (iRes == AVERROR_EOF)
					{
						av.m_finished = av.m_serial;
						avcodec_flush_buffers(av.m_context);
						return 0;
					}
					if (iRes >= 0)
						return 1;
				} while (iRes != AVERROR(EAGAIN));
			}
			do
			{
				if (av.m_queue.m_packets.m_linkqueue.size() == 0)
				{
					if (av.m_continueCV != NULL)
						av.m_continueCV->Unlock(FALSE);
				}
				if (av.m_bPending)
				{
					av_packet_move_ref(&pkt, &av.m_pkt);
					av.m_bPending = FALSE;
				}
				else
				{
					if (!av.m_queue.m_packets.Pop(pkt, &av.m_serial))
						return -1;
				}
				if (av.m_queue.m_packets.m_serial == av.m_serial)
					break;
				av_packet_unref(&pkt);
			} while (1);
			if (pkt.data == g_packet.data)
			{
				avcodec_flush_buffers(av.m_context);
				av.m_finished = 0;
				av.m_pts = AV_NOPTS_VALUE;
				av.m_rational = { 0, 0 };
			}
			else
			{
				if (avcodec_send_packet(av.m_context, &pkt) == AVERROR(EAGAIN))
				{
					av.m_bPending = TRUE;
					av_packet_move_ref(&av.m_pkt, &pkt);
				}
				av_packet_unref(&pkt);
			}
		}
	}

	BOOL AVDemuxer::CheckPackets(AVStream* stream, INT32 streamID, AVPacketQueue* queue)
	{
		return streamID < 0 || queue->m_stop || (stream->disposition & AV_DISPOSITION_ATTACHED_PIC) ||
			queue->m_linkqueue.size() > MIN_FRAMES &&
			(!queue->m_duration || av_q2d(stream->time_base) * queue->m_duration > 1.0);
	}
} // namespace MediaSDK
