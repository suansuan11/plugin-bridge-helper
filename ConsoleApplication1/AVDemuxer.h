#pragma once
#include <windows.h>
#include "Common.h"
#include "Resampler.h"
#include "ByteQueue.h"

namespace IPCDemo
{


	typedef struct tagFF_AV_PACKET
	{
		INT32    serial;
		AVPacket pkt;
	} FF_AV_PACKET;

	typedef struct tagFF_AV_FRAME
	{
		AVFrame*           frame;
		INT32              serial;
		INT32              width;
		INT32              height;
		INT32              format;
		INT64              pos;
		DOUBLE             pts;
		DOUBLE             duration;
		AVRational         rational;
	} FF_AV_FRAME;

	class AVPacketQueue
	{
	public:
		AVPacketQueue();
		~AVPacketQueue();

	public:
		BOOL Pop(AVPacket& pkt, INT32* serial);
		BOOL Push(INT32 index);
		BOOL Push(AVPacket& pkt);
		void Flush();
		void Unlock();
		int  PacketCount();
		int  GetBufferedAudioPacketDurationMS();

	public:
		volatile BOOL            m_stop;
		INT32                    m_serial;
		INT32                    m_size;
		INT64                    m_duration;
		MyLock                   m_mutex;
		MyConditionVariable      m_cv;
		std::deque<FF_AV_PACKET> m_linkqueue;
	};

	class AVFrameQueue
	{
	public:
		AVFrameQueue();
		~AVFrameQueue();

	public:
		INT32        remaining() const;
		FF_AV_FRAME* Peek(BOOL bREAD = FALSE);
		FF_AV_FRAME* PeekNext();
		FF_AV_FRAME* PeakLast();
		void         Push();
		BOOL         Next();
		INT64        GetLastPos();
		void         Unlock();
		BOOL         Create(INT32 maxsize);
		void         Destroy();

	public:
		INT32                 m_indexR;
		INT32                 m_indexW;
		INT32                 m_indexshow;
		INT32                 m_size;
		INT32                 m_maxsize;
		FF_AV_FRAME           m_frames[FRAME_QUEUE_SIZE];
		AVPacketQueue         m_packets;
		MyLock              m_mutex;
		MyConditionVariable m_cv;
	};

	class FF_AV
	{
	public:
		FF_AV();
		~FF_AV();
		AVStream* stream();
		BOOL      Open(AVFormatContext* context, INT32 index, BOOL& bEnableHW);
		void      Close();
		void      Stop();
		void      Unlock();

	public:
		BOOL                   m_bPending;
		BOOL                   m_bEnableHW;
		INT32                  m_finished;
		INT32                  m_serial;
		INT32                  m_index;
		INT64                  m_pts;
		AVRational             m_rational;
		AVPacket               m_pkt;
		AVFrameQueue           m_queue;
		AVStream*              m_stream;
		AVBufferRef*           m_contextHW;
		AVCodecContext*        m_context;
		MyConditionVariable* m_continueCV;
		enum AVPixelFormat     m_format;
		std::thread             m_task;
	};

	class Clock
	{
	public:
		Clock();
		~Clock();
		void   SetupClock(INT32* queue_serial);
		DOUBLE GetClock();
		void   SetClock(double pts, int serial);
		void   SetClockAt(double pts, int serial, double time);
		void   SetClockSpeed(double speed);

	public:
		BOOL   paused;
		DOUBLE pts;
		DOUBLE pts_drift;
		DOUBLE last_updateds;
		DOUBLE speed;
		INT32  serial;
		INT32* queueserial;
	};

	class AVDemuxer
	{
	public:
		AVDemuxer();
		~AVDemuxer();

	public:
		BOOL   IsExiting() const;

	public:
		BOOL        Open(const std::string& szNAME);
		void        Close();

	public:
		using AUDIO_START_CALLBACK = std::function<void(const AudioParameter&)>;
		using AUDIO_PACKET_CALLBACK = std::function<void(IPC_PACKET&)>;
		using VIDEO_PACKET_CALLBACK = std::function<void(IPC_PACKET&)>;

		AUDIO_START_CALLBACK	AUDIO_START;
		AUDIO_PACKET_CALLBACK	AUDIO_PACKET;
		VIDEO_PACKET_CALLBACK	VIDEO_PACKET;

	private:
		BOOL  CheckPackets(AVStream* stream, INT32 streamID, AVPacketQueue* queue);
		INT32 GetAudioFrame(UINT8* output[], INT32& serial, DOUBLE& audioPTS, INT32 chunksize);
		INT32 GetVideoFrame(AVFrame* video);

	private:
		BOOL   InitializeFilter(AVFrame* video);
		void   UninitializeFilter();
		double CalculateDelay(double delay);
		double CalculateDuration(FF_AV_FRAME* vp, FF_AV_FRAME* nextvp);
		void   OnReadPump();
		void   OnAudioDecodePump();
		void   OnVideoDecodePump();
		void   OnAudioRenderPump();
		BOOL   OnVideoRender(double* remaining);
		void   OnVideoRenderPump();
		INT32  OnQueuePicture(AVFrame* video, DOUBLE pts, DOUBLE duration, INT64 pos, INT32 serial);
		INT32  OnDecodeFrame(FF_AV& av, AVFrame* frame);

	private:
		std::string                       m_visualID;
		volatile std::atomic<BOOL>        m_exiting;
		volatile BOOL                     m_bEOF;
		BOOL                              m_bStep;
		BOOL                              m_bPause;
		BOOL                              m_bPreviousPause;
		BOOL                              m_bAttachment;
		BOOL                              m_bEnableLoop;
		BOOL                              m_bEnableHW;
		BOOL                              m_bPresent;
		INT32                             m_timeout;
		INT32                             m_reconnects;
		INT32                             m_buffersize;
		INT64                             m_beginTS;
		INT64                             m_timestamp;
		INT64                             m_seekPos;
		INT64                             m_seekRel;
		INT32                             m_seekFlag;
		Clock                             m_audioclock;
		Clock                             m_videoclock;
		DOUBLE                            m_lasttime;
		DOUBLE                            m_lastdelay;
		DOUBLE                            m_previousPos;
		DOUBLE                            m_maxduration;
		DOUBLE                            m_audioPTS;
		DOUBLE                            m_videoPTS;
		std::string                       m_szNAME;
		AudioResampler                    m_resampler;
		AVFilterGraph*                    m_graph;
		AVFormatContext*                  m_context;
		AVFilterContext*                  m_yadifFilter = NULL;
		AVFilterContext*                  m_bufferFilter = NULL;
		AVFilterContext*                  m_buffersinkFilter = NULL;
		FF_AV                             m_audio;
		FF_AV                             m_video;
		MyLock                          m_mutexCV;
		MyConditionVariable             m_continueCV;
		std::thread                       m_readTask;
		std::thread                       m_audioTask;
		std::thread                       m_videoTask;
		ByteQueue                       m_aq;
		SwsContext*                       m_swscontext;
		AudioParameter                    m_audioParameterI;
		AudioParameter                    m_audioParameterO;
		VideoParameter                    m_videoParameter;
		AVPixelFormat                     m_bestFMT;
		UINT8*                            m_pointers[4];
		INT32                             m_linesizes[4];
		int32_t                           m_errorEofRetryCnt{ 0 };
		INT64                             m_lastRetryNS = 0;
	};
} // namespace MediaSDK
