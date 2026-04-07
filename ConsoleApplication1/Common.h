#pragma once
#include <windows.h>
#include <d3d11.h>
#include <mmeapi.h>
#include <ks.h>
#include <ksmedia.h>

extern "C"
{
#include "libswscale/swscale.h"
#include "libavutil/avutil.h"
#include "libavutil/mem.h"
#include "libavutil/avstring.h"
#include "libavutil/eval.h"
#include "libavutil/mathematics.h"
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"
#include "libavutil/dict.h"
#include "libavutil/parseutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/avassert.h"
#include "libavutil/time.h"
#include "libavutil/mem.h"
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libavutil/opt.h"
#include "libavcodec/avfft.h"
#include "libswresample/swresample.h"
#include "libavformat/avformat.h"
#include "libswresample/swresample.h"
#include "libavutil/common.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/log.h"
#include "libavutil/error.h"
#include "libavutil/avassert.h"
#include "libavcodec/avcodec.h"
#include "libavcodec/dxva2.h"
#include "libavcodec/d3d11va.h"
#include "libavcodec/avcodec.h"
#include "libavcodec/bsf.h"
#include "libavcodec/codec.h"
#include "libavcodec/codec_desc.h"
#include "libavcodec/version.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"

}
#include <vector>
#include <queue>
#include <deque>
#include <atomic>
#include <string>
#include <thread>
#include <memory>
#include <functional>
#include "PipeDef.h"
#pragma comment(lib,"PipeSDK.lib")


using namespace std;
using namespace PipeSDK;

#pragma warning(disable: 4996)

#ifndef KSAUDIO_SPEAKER_2POINT1
#define KSAUDIO_SPEAKER_2POINT1 (KSAUDIO_SPEAKER_STEREO | SPEAKER_LOW_FREQUENCY)
#endif // !KSAUDIO_SPEAKER_2POINT1

#ifndef KSAUDIO_SPEAKER_3POINT1
#define KSAUDIO_SPEAKER_3POINT1 (KSAUDIO_SPEAKER_STEREO | SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY)
#endif // !KSAUDIO_SPEAKER_3POINT1

#ifndef KSAUDIO_SPEAKER_4POINT1
#define KSAUDIO_SPEAKER_4POINT1 (KSAUDIO_SPEAKER_QUAD | SPEAKER_LOW_FREQUENCY)
#endif // !KSAUDIO_SPEAKER_4POINT1

#define MAX_AUDIO_BUFFER_SIZE (1024 * 1024 * 8)
#define MAX_AV_PLANES 8
#define MAX_AUDIO_CHANNELS MAX_AV_PLANES
#define MAX_VIDEO_SIZE (1024 * 1024 * 12)
#define MAX_QUEUE_SIZE (1024 * 1024 * 15)
#define REFRESH_RATE 0.01
#define AV_SYNC_THRESHOLD_MIN 0.04
#define AV_SYNC_THRESHOLD_MAX 0.1
#define AV_SYNC_FRAMEDUP_THRESHOLD 0.1
#define AV_NOSYNC_THRESHOLD 10.0
#define MIN_FRAMES 25
#define EXTERNAL_CLOCK_MIN_FRAMES 2
#define EXTERNAL_CLOCK_MAX_FRAMES 10
#define VIDEO_QUEUE_SIZE 3
#define AUDIO_QUEUE_SIZE 9
#define FRAME_QUEUE_SIZE FFMAX(AUDIO_QUEUE_SIZE, VIDEO_QUEUE_SIZE)

class MyLock
{
public:
	MyLock() throw();
	virtual ~MyLock();
	operator CRITICAL_SECTION&();
	BOOL SetSpinCount(DWORD dwSpinCount);
	BOOL TryLock() throw();
	void Lock() throw();
	void Unlock() throw();
private:
	CRITICAL_SECTION m_s;
};


class MyConditionVariable
{
public:
	MyConditionVariable();
	~MyConditionVariable();
	void Lock(CRITICAL_SECTION& cs);
	void Lock(SRWLOCK& lock);
	BOOL TryLock(CRITICAL_SECTION& cs, DWORD dwMS = INFINITE);
	BOOL TryLock(SRWLOCK& lock, DWORD dwMS = INFINITE);
	void Unlock(BOOL bAll);

private:
	SRWLOCK            m_lock;
	CONDITION_VARIABLE m_cv;
	CRITICAL_SECTION   m_cs;
};
class AutoLock
{
public:
	explicit AutoLock(MyLock& lock);
	virtual ~AutoLock();

private:
	MyLock& m_lock;
};

class VideoParameter
{
public:
	VideoParameter();
	~VideoParameter();
	BOOL               operator!=(const VideoParameter& vps);
	VideoParameter&    operator=(const VideoParameter& vps);
	IPC_VIDEO_PIXEL_FORMAT GetFormat() const;
	IPC_COLOR_SPACE        GetCS() const;
	IPC_COLOR_TRANSFER     GetCT() const;
	IPC_VIDEO_RANGE        GetRange() const;
	SIZE				GetSize() const;
	void				SetSize(const SIZE& size);
	void               SetFormat(IPC_VIDEO_PIXEL_FORMAT vFormat);
	void               SetRange(IPC_VIDEO_RANGE vRange);
	void               SetCS(IPC_COLOR_SPACE cs);
	void               SetCT(IPC_COLOR_TRANSFER ct);
	IPC_COLOR_SPACE        m_cs;
	IPC_VIDEO_PIXEL_FORMAT m_format;
	IPC_VIDEO_RANGE        m_range;
	IPC_COLOR_TRANSFER     m_ct;
	SIZE				m_size;
};
class AudioParameter
{
public:
	AudioParameter();
	AudioParameter(const AudioParameter& param);
	AudioParameter(AudioParameter&& param);
	AudioParameter& operator=(const AudioParameter& param);
	AudioParameter& operator=(AudioParameter&& param);
	bool            operator==(const AudioParameter& param) const;
	bool            operator!=(const AudioParameter& param) const;
	void            swap(AudioParameter& param);
	~AudioParameter();
	WORD                GetChannels() const;
	WORD                GetBitsPerSample() const;
	WORD                GetFrames() const;
	WORD                GetPlanes() const;
	DWORD               GetSamplesPerSec() const;
	IPC_CHANNEL_LAYOUT      GetChannelLayout() const;
	IPC_AUDIO_FORMAT        GetAudioFormat() const;
	void                SetSamplesPerSec(DWORD dwSamplesPerSec);
	void                SetBitsPerSample(WORD wBitsPerSample);
	void                SetChannels(WORD wChannels);
	void                SetFrames(WORD wFrames);
	void                SetPlanes(WORD wPlanes);
	void                SetAudioFormat(IPC_AUDIO_FORMAT s);
	void                SetChannelLayout(IPC_CHANNEL_LAYOUT s);
	DWORD               GetBlockSize();

private:
	DWORD                  m_dwSamplesPerSec;
	WORD                   m_wBitsPerSample;
	WORD                   m_wChannels;
	WORD                   m_wFrames;
	WORD                   m_wPlanes;
	IPC_AUDIO_FORMAT       m_audioF;
	IPC_CHANNEL_LAYOUT     m_channelLayout;
};

inline INT32 MS_AV_CS_TO_SWS_CS(enum AVColorSpace cs)
{
	switch (cs)
	{
	case AVCOL_SPC_BT2020_CL:
	case AVCOL_SPC_BT2020_NCL:
		return SWS_CS_BT2020;
	case AVCOL_SPC_SMPTE170M:
		return SWS_CS_SMPTE170M;
	case AVCOL_SPC_BT709:
		return SWS_CS_ITU709;
	case AVCOL_SPC_SMPTE240M:
		return SWS_CS_SMPTE240M;
	case AVCOL_SPC_FCC:
		return SWS_CS_FCC;
	default:
		break;
	}
	return SWS_CS_ITU601;
}
inline IPC_VIDEO_PIXEL_FORMAT MS_AV_PIX_FMT_TO_VIDEO_PIXEL_FORMAT(enum AVPixelFormat format)
{
	switch (format)
	{
	case AV_PIX_FMT_YUV420P:
	case AV_PIX_FMT_YUVJ420P:
		return IPC_PIXEL_FORMAT_I420;
	case AV_PIX_FMT_YUVA420P:
		return IPC_PIXEL_FORMAT_I420A;
	case AV_PIX_FMT_NV12:
		return IPC_PIXEL_FORMAT_NV12;
	case AV_PIX_FMT_NV21:
		return IPC_PIXEL_FORMAT_NV21;
	case AV_PIX_FMT_YUYV422:
		return IPC_PIXEL_FORMAT_YUY2;
	case AV_PIX_FMT_UYVY422:
		return IPC_PIXEL_FORMAT_UYVY;
	case AV_PIX_FMT_YUV422P:
	case AV_PIX_FMT_YUVJ422P:
		return IPC_PIXEL_FORMAT_I422;
	case AV_PIX_FMT_YUVA422P:
		return IPC_PIXEL_FORMAT_I422A;
	case AV_PIX_FMT_YUV444P:
		return IPC_PIXEL_FORMAT_I444;
	case AV_PIX_FMT_YUVA444P:
		return IPC_PIXEL_FORMAT_I444A;
	case AV_PIX_FMT_ARGB:
		return IPC_PIXEL_FORMAT_ARGB;
	case AV_PIX_FMT_BGR24:
		return IPC_PIXEL_FORMAT_BGR24;
	case AV_PIX_FMT_BGRA:
		return IPC_PIXEL_FORMAT_BGRA;
	case AV_PIX_FMT_RGB24:
		return IPC_PIXEL_FORMAT_RGB24;
	case AV_PIX_FMT_RGBA:
		return IPC_PIXEL_FORMAT_RGBA;
	}
	return IPC_PIXEL_FORMAT_UNKNOWN;
}

inline AVPixelFormat MS_VIDEO_PIXEL_FORMAT_TO_AV_PIX_FMT(IPC_VIDEO_PIXEL_FORMAT format)
{
	switch (format)
	{
	case IPC_PIXEL_FORMAT_I420:
		return AV_PIX_FMT_YUV420P;
	case IPC_PIXEL_FORMAT_NV12:
		return AV_PIX_FMT_NV12;
	case IPC_PIXEL_FORMAT_NV21:
		return AV_PIX_FMT_NV21;
	case IPC_PIXEL_FORMAT_YUY2:
		return AV_PIX_FMT_YUYV422;
	case IPC_PIXEL_FORMAT_UYVY:
		return AV_PIX_FMT_UYVY422;
	case IPC_PIXEL_FORMAT_I444:
		return AV_PIX_FMT_YUV444P;
	case IPC_PIXEL_FORMAT_I422:
		return AV_PIX_FMT_YUV422P;
	case IPC_PIXEL_FORMAT_ARGB:
		return AV_PIX_FMT_ARGB;
	case IPC_PIXEL_FORMAT_RGB24:
		return AV_PIX_FMT_RGB24;
	case IPC_PIXEL_FORMAT_BGR24:
		return AV_PIX_FMT_BGR24;
	case IPC_PIXEL_FORMAT_RGBA:
		return AV_PIX_FMT_RGBA;
	case IPC_PIXEL_FORMAT_BGRA:
		return AV_PIX_FMT_BGRA;
	}
	return AV_PIX_FMT_NONE;
}

inline IPC_AUDIO_FORMAT MS_AV_SAMPLE_FMT_TO_AUDIO_FORMAT(AVSampleFormat format)
{
	switch (format)
	{
	case AV_SAMPLE_FMT_U8:
		return IPC_AUDIO_FORMAT_U8BIT;
	case AV_SAMPLE_FMT_S16:
		return IPC_AUDIO_FORMAT_16BIT;
	case AV_SAMPLE_FMT_S32:
		return IPC_AUDIO_FORMAT_32BIT;
	case AV_SAMPLE_FMT_FLT:
		return IPC_AUDIO_FORMAT_FLOAT;
	case AV_SAMPLE_FMT_U8P:
		return IPC_AUDIO_FORMAT_U8BIT_PLANAR;
	case AV_SAMPLE_FMT_S16P:
		return IPC_AUDIO_FORMAT_16BIT_PLANAR;
	case AV_SAMPLE_FMT_S32P:
		return IPC_AUDIO_FORMAT_32BIT_PLANAR;
	case AV_SAMPLE_FMT_FLTP:
		return IPC_AUDIO_FORMAT_FLOAT_PLANAR;
	}
	return IPC_AUDIO_FORMAT_UNKNOWN;
}
inline INT32 MS_AV_SAMPLE_FMT_TO_SIZE(AVSampleFormat format)
{
	switch (format)
	{
	case AV_SAMPLE_FMT_U8:
	case AV_SAMPLE_FMT_U8P:
		return 8;
	case AV_SAMPLE_FMT_S16:
	case AV_SAMPLE_FMT_S16P:
		return 16;
	case AV_SAMPLE_FMT_S32:
	case AV_SAMPLE_FMT_FLT:
	case AV_SAMPLE_FMT_S32P:
	case AV_SAMPLE_FMT_FLTP:
		return 32;
	}
	return 0;
}
inline BOOL IsAudioPlanar(IPC_AUDIO_FORMAT format)
{
	switch (format)
	{
	case IPC_AUDIO_FORMAT_UNKNOWN:
	case IPC_AUDIO_FORMAT_U8BIT:
	case IPC_AUDIO_FORMAT_16BIT:
	case IPC_AUDIO_FORMAT_32BIT:
	case IPC_AUDIO_FORMAT_FLOAT:
		return FALSE;
	case IPC_AUDIO_FORMAT_U8BIT_PLANAR:
	case IPC_AUDIO_FORMAT_16BIT_PLANAR:
	case IPC_AUDIO_FORMAT_32BIT_PLANAR:
	case IPC_AUDIO_FORMAT_FLOAT_PLANAR:
		return TRUE;
	}
	return FALSE;
}
inline AVSampleFormat MS_AUDIO_FORMAT_TO_AV_SAMPLE_FMT(IPC_AUDIO_FORMAT format)
{
	switch (format)
	{
	case IPC_AUDIO_FORMAT_U8BIT:
		return AV_SAMPLE_FMT_U8;
	case IPC_AUDIO_FORMAT_16BIT:
		return AV_SAMPLE_FMT_S16;
	case IPC_AUDIO_FORMAT_32BIT:
		return AV_SAMPLE_FMT_S32;
	case IPC_AUDIO_FORMAT_FLOAT:
		return AV_SAMPLE_FMT_FLT;
	case IPC_AUDIO_FORMAT_U8BIT_PLANAR:
		return AV_SAMPLE_FMT_U8P;
	case IPC_AUDIO_FORMAT_16BIT_PLANAR:
		return AV_SAMPLE_FMT_S16P;
	case IPC_AUDIO_FORMAT_32BIT_PLANAR:
		return AV_SAMPLE_FMT_S32P;
	case IPC_AUDIO_FORMAT_FLOAT_PLANAR:
		return AV_SAMPLE_FMT_FLTP;
	}
	return AV_SAMPLE_FMT_NONE;
}

inline IPC_CHANNEL_LAYOUT CHANNEL_TO_CHANNEL_LAYOUT(INT channel)
{
	switch (channel)
	{
	case 1:
		return IPC_CHANNEL_MONO;
	case 2:
		return IPC_CHANNEL_STEREO;
	case 3:
		return IPC_CHANNEL_2POINT1;
	case 4:
		return IPC_CHANNEL_4POINT0;
	case 5:
		return IPC_CHANNEL_4POINT1;
	case 6:
		return IPC_CHANNEL_5POINT1;
	case 7:
		return IPC_CHANNEL_6POINT1;
	case 8:
		return IPC_CHANNEL_7POINT1;
	default:
		return IPC_CHANNEL_UNKNOWN;
	}
}


inline IPC_CHANNEL_LAYOUT MS_AV_CH_LAYOUT_TO_CHANNEL_LAYOUT(INT64 layout)
{
	switch (layout)
	{
	case AV_CH_LAYOUT_MONO:
		return IPC_CHANNEL_MONO;
	case AV_CH_LAYOUT_STEREO:
		return IPC_CHANNEL_STEREO;
	case AV_CH_LAYOUT_2_1:
		return IPC_CHANNEL_2_1;
	case AV_CH_LAYOUT_2_2:
		return IPC_CHANNEL_2_2;
	case AV_CH_LAYOUT_2POINT1:
		return IPC_CHANNEL_2POINT1;
	case AV_CH_LAYOUT_3POINT1:
		return IPC_CHANNEL_3POINT1;
	case AV_CH_LAYOUT_SURROUND:
		return IPC_CHANNEL_SURROUND;
	case AV_CH_LAYOUT_QUAD:
		return IPC_CHANNEL_QUAD;
	case AV_CH_LAYOUT_4POINT0:
		return IPC_CHANNEL_4POINT0;
	case AV_CH_LAYOUT_4POINT1:
		return IPC_CHANNEL_4POINT1;
	case AV_CH_LAYOUT_5POINT0:
		return IPC_CHANNEL_5POINT0;
	case AV_CH_LAYOUT_5POINT0_BACK:
		return IPC_CHANNEL_5POINT0_BACK;
	case AV_CH_LAYOUT_5POINT1:
		return IPC_CHANNEL_5POINT1;
	case AV_CH_LAYOUT_5POINT1_BACK:
		return IPC_CHANNEL_5POINT1_BACK;
	case AV_CH_LAYOUT_6POINT0:
		return IPC_CHANNEL_6POINT0;
	case AV_CH_LAYOUT_6POINT1:
		return IPC_CHANNEL_6POINT1;
	case AV_CH_LAYOUT_7POINT0:
		return IPC_CHANNEL_7POINT0;
	case AV_CH_LAYOUT_7POINT0_FRONT:
		return IPC_CHANNEL_7POINT0_FRONT;
	case AV_CH_LAYOUT_7POINT1:
		return IPC_CHANNEL_7POINT1;
	case AV_CH_LAYOUT_7POINT1_WIDE:
		return IPC_CHANNEL_7POINT1_WIDE;
	case AV_CH_LAYOUT_7POINT1_WIDE_BACK:
		return IPC_CHANNEL_7POINT1_WIDE_BACK;
	}
	return IPC_CHANNEL_UNKNOWN;
}

inline IPC_COLOR_SPACE MS_AVCOL_SPC_TO_COLOR_SPACE(AVColorSpace cs)
{
	switch (cs)
	{
	case AVCOL_SPC_FCC:
	case AVCOL_SPC_BT470BG:
	case AVCOL_SPC_SMPTE170M:
	case AVCOL_SPC_SMPTE240M:
		return IPC_COLOR_SPACE_BT601;
	case AVCOL_SPC_BT709:
		return IPC_COLOR_SPACE_BT709;
	case AVCOL_SPC_BT2020_CL:
	case AVCOL_SPC_BT2020_NCL:
		return IPC_COLOR_SPACE_BT2020;
	}
	return IPC_COLOR_SPACE_UNSPECIFIED;
}

inline AVColorTransferCharacteristic MS_COLOR_TRANSFER_TO_AVCOL_TRC(IPC_COLOR_TRANSFER ct)
{
	switch (ct)
	{
	case IPC_COLOR_TRANSFER_LINEAR:
		return AVCOL_TRC_LINEAR;
	case IPC_COLOR_TRANSFER_GAMMA22:
		return AVCOL_TRC_GAMMA22;
	case IPC_COLOR_TRANSFER_GAMMA28:
		return AVCOL_TRC_GAMMA28;
	case IPC_COLOR_TRANSFER_SMPTE170M:
		return AVCOL_TRC_SMPTE170M;
	case IPC_COLOR_TRANSFER_SMPTE240M:
		return AVCOL_TRC_SMPTE240M;
	case IPC_COLOR_TRANSFER_BT709:
		return AVCOL_TRC_BT709;
	case IPC_COLOR_TRANSFER_BT2020_10:
		return AVCOL_TRC_BT2020_10;
	case IPC_COLOR_TRANSFER_BT2020_12:
		return AVCOL_TRC_BT2020_12;
	case IPC_COLOR_TRANSFER_SMPTE2084:
		return AVCOL_TRC_SMPTE2084;
	case IPC_COLOR_TRANSFER_ARIB_STD_B67:
		return AVCOL_TRC_ARIB_STD_B67;
	}
	return AVCOL_TRC_BT709;
}
inline IPC_COLOR_TRANSFER MS_AVCOL_SPC_TO_COLOR_TRANSFER(AVColorTransferCharacteristic ct)
{
	switch (ct)
	{
	case AVCOL_TRC_BT709:
		return IPC_COLOR_TRANSFER_BT709;
	case AVCOL_TRC_GAMMA22:
		return IPC_COLOR_TRANSFER_GAMMA22;
	case AVCOL_TRC_GAMMA28:
		return IPC_COLOR_TRANSFER_GAMMA28;
	case AVCOL_TRC_SMPTE170M:
		return IPC_COLOR_TRANSFER_SMPTE170M;
	case AVCOL_TRC_SMPTE240M:
		return IPC_COLOR_TRANSFER_SMPTE240M;
	case AVCOL_TRC_SMPTE2084:
		return IPC_COLOR_TRANSFER_SMPTE2084;
	case AVCOL_TRC_ARIB_STD_B67:
		return IPC_COLOR_TRANSFER_ARIB_STD_B67;
	}
	return IPC_COLOR_TRANSFER_UNSPECIFIED;
}
inline IPC_VIDEO_RANGE MS_AVCOL_RNG_TO_VIDEO_RANGE(AVColorRange range)
{
	IPC_VIDEO_RANGE videoRange = IPC_VIDEO_RANGE_UNSPECIFIED;
	switch (range)
	{
	case AVCOL_RANGE_UNSPECIFIED:
		break;
	case AVCOL_RANGE_MPEG:
		videoRange = IPC_VIDEO_RANGE_PARTIAL;
		break;
	case AVCOL_RANGE_JPEG:
		videoRange = IPC_VIDEO_RANGE_FULL;
		break;
	case AVCOL_RANGE_NB:
		break;
	default:
		break;
	}
	return videoRange;
}
