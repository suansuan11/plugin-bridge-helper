#include "Common.h"

MyLock::MyLock() throw()
{
	ZeroMemory(&m_s, sizeof(m_s));
	InitializeCriticalSection(&m_s);
	SetCriticalSectionSpinCount(&m_s, 4000);
}

MyLock::~MyLock()
{
	::DeleteCriticalSection(&m_s);
}

BOOL MyLock::SetSpinCount(DWORD dwSpinCount)
{
	return ::SetCriticalSectionSpinCount(&m_s, dwSpinCount);
}

BOOL MyLock::TryLock() throw()
{
	return ::TryEnterCriticalSection(&m_s);
}

void MyLock::Lock() throw()
{
	::EnterCriticalSection(&m_s);
}

void MyLock::Unlock() throw()
{
	::LeaveCriticalSection(&m_s);
}

MyLock::operator CRITICAL_SECTION&()
{
	return m_s;
}
//////////////////////////////////////////////////////////////////////////
MyConditionVariable::MyConditionVariable()
{
	InitializeConditionVariable(&m_cv);
}

MyConditionVariable::~MyConditionVariable()
{
}

void MyConditionVariable::Lock(CRITICAL_SECTION& cs)
{
	SleepConditionVariableCS(&m_cv, &cs, INFINITE);
}

void MyConditionVariable::Lock(SRWLOCK& lock)
{
	SleepConditionVariableSRW(&m_cv, &lock, INFINITE, 0);
}

BOOL MyConditionVariable::TryLock(CRITICAL_SECTION& cs, DWORD dwMS)
{
	return SleepConditionVariableCS(&m_cv, &cs, dwMS);
}

BOOL MyConditionVariable::TryLock(SRWLOCK& lock, DWORD dwMS)
{
	return SleepConditionVariableSRW(&m_cv, &lock, dwMS, 0);
}

void MyConditionVariable::Unlock(BOOL bAll)
{
	if (bAll)
	{
		WakeAllConditionVariable(&m_cv);
	}
	else
	{
		WakeConditionVariable(&m_cv);
	}
}
//////////////////////////////////////////////////////////////////////////
AutoLock::AutoLock(MyLock& lock)
	: m_lock(lock)
{
	m_lock.Lock();
}

AutoLock::~AutoLock()
{
	m_lock.Unlock();
}
//////////////////////////////////////////////////////////////////////////
AudioParameter::AudioParameter()
	: m_audioF(IPC_AUDIO_FORMAT_16BIT)
	, m_channelLayout(IPC_CHANNEL_STEREO)
	, m_wFrames(0)
	, m_wPlanes(0)
	, m_dwSamplesPerSec(44100)
	, m_wBitsPerSample(16)
	, m_wChannels(2)
{

}

AudioParameter::~AudioParameter()
{
}

AudioParameter::AudioParameter(const AudioParameter& param)
{
	m_audioF = param.m_audioF;
	m_channelLayout = param.m_channelLayout;
	m_dwSamplesPerSec = param.m_dwSamplesPerSec;
	m_wBitsPerSample = param.m_wBitsPerSample;
	m_wChannels = param.m_wChannels;
	m_wFrames = param.m_wFrames;
	m_wPlanes = param.m_wPlanes;
}

AudioParameter::AudioParameter(AudioParameter&& param)
	: m_wFrames(param.m_wFrames)
	, m_wPlanes(param.m_wPlanes)
	, m_dwSamplesPerSec(param.m_dwSamplesPerSec)
	, m_wChannels(param.m_wChannels)
	, m_wBitsPerSample(param.m_wBitsPerSample)
	, m_channelLayout(param.m_channelLayout)
	, m_audioF(param.m_audioF)
{
	param.m_audioF = IPC_AUDIO_FORMAT_UNKNOWN;
	param.m_channelLayout = IPC_CHANNEL_UNKNOWN;
	param.m_wFrames = 0;
	param.m_dwSamplesPerSec = 0;
	param.m_wBitsPerSample = 0;
	param.m_wChannels = 0;
}

void AudioParameter::swap(AudioParameter& param)
{
	std::swap(m_dwSamplesPerSec, param.m_dwSamplesPerSec);
	std::swap(m_wBitsPerSample, param.m_wBitsPerSample);
	std::swap(m_wChannels, param.m_wChannels);
	std::swap(m_wFrames, param.m_wFrames);
	std::swap(m_wPlanes, param.m_wPlanes);
	std::swap(m_audioF, param.m_audioF);
	std::swap(m_channelLayout, param.m_channelLayout);
}

bool AudioParameter::operator==(const AudioParameter& param) const
{
	return (m_audioF == param.m_audioF && m_channelLayout == param.m_channelLayout && m_wChannels == param.m_wChannels &&
		m_dwSamplesPerSec == param.m_dwSamplesPerSec && m_wBitsPerSample == param.m_wBitsPerSample);
}

bool AudioParameter::operator!=(const AudioParameter& param) const
{
	return (m_audioF != param.m_audioF || m_channelLayout != param.m_channelLayout || m_wChannels != param.m_wChannels ||
		m_dwSamplesPerSec != param.m_dwSamplesPerSec || m_wBitsPerSample != param.m_wBitsPerSample);
}

AudioParameter& AudioParameter::operator=(const AudioParameter& param)
{
	if (&param != this)
	{
		m_audioF = param.m_audioF;
		m_channelLayout = param.m_channelLayout;
		m_wFrames = param.m_wFrames;
		m_wPlanes = param.m_wPlanes;
		m_wChannels = param.m_wChannels;
		m_dwSamplesPerSec = param.m_dwSamplesPerSec;
		m_wBitsPerSample = param.m_wBitsPerSample;
	}
	return *this;
}

AudioParameter& AudioParameter::operator=(AudioParameter&& param)
{
	if (&param != this)
	{
		m_audioF = param.m_audioF;
		m_channelLayout = param.m_channelLayout;
		m_wFrames = param.m_wFrames;
		m_wPlanes = param.m_wPlanes;
		m_wChannels = param.m_wChannels;
		m_dwSamplesPerSec = param.m_dwSamplesPerSec;
		m_wBitsPerSample = param.m_wBitsPerSample;
	}
	return *this;
}

void AudioParameter::SetSamplesPerSec(DWORD dwSamplesPerSec)
{
	m_dwSamplesPerSec = dwSamplesPerSec;
}

DWORD AudioParameter::GetSamplesPerSec() const
{
	return m_dwSamplesPerSec;
}

void AudioParameter::SetBitsPerSample(WORD wBitsPerSample)
{
	m_wBitsPerSample = wBitsPerSample;
}

WORD AudioParameter::GetBitsPerSample() const
{
	return m_wBitsPerSample;
}

void AudioParameter::SetChannels(WORD wChannels)
{
	m_wChannels = wChannels;
	m_wPlanes = IsAudioPlanar(m_audioF) ? m_wChannels : 1;
}

WORD AudioParameter::GetChannels() const
{
	return m_wChannels;
}

void AudioParameter::SetFrames(WORD wFrames)
{
	m_wFrames = wFrames;
}

DWORD AudioParameter::GetBlockSize()
{
	return static_cast<UINT32>(GetChannels() * (GetBitsPerSample() / 8));
}

WORD AudioParameter::GetFrames() const
{
	return m_wFrames;
}

void AudioParameter::SetPlanes(WORD wPlanes)
{
	m_wPlanes = wPlanes;
}

WORD AudioParameter::GetPlanes() const
{
	return m_wPlanes;
}

static inline IPC_CHANNEL_LAYOUT MS_CHANNEL_TO_CHANNEL_LAYOUT(INT32 cahnnel)
{
	switch (cahnnel)
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
inline IPC_CHANNEL_LAYOUT ConvertChannelLayout(const WAVEFORMATEX* pFMT)
{
	if (!pFMT)
		return IPC_CHANNEL_UNKNOWN;
	if (pFMT->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
	{
		WAVEFORMATEXTENSIBLE* s = (WAVEFORMATEXTENSIBLE*)pFMT;
		switch (s->dwChannelMask)
		{
		case KSAUDIO_SPEAKER_QUAD:
			return IPC_CHANNEL_QUAD;
		case KSAUDIO_SPEAKER_2POINT1:
			return IPC_CHANNEL_2POINT1;
		case KSAUDIO_SPEAKER_3POINT1:
			return IPC_CHANNEL_3POINT1;
		case KSAUDIO_SPEAKER_4POINT1:
			return IPC_CHANNEL_4POINT1;
		case KSAUDIO_SPEAKER_SURROUND:
			return IPC_CHANNEL_SURROUND;
		case KSAUDIO_SPEAKER_5POINT1:
			return IPC_CHANNEL_5POINT1;
		case KSAUDIO_SPEAKER_5POINT1_SURROUND:
			return IPC_CHANNEL_5POINT1_BACK;
		case KSAUDIO_SPEAKER_7POINT1:
			return IPC_CHANNEL_7POINT1;
		case KSAUDIO_SPEAKER_7POINT1_SURROUND:
			return IPC_CHANNEL_7POINT1_WIDE;
		}
	}
	return CHANNEL_TO_CHANNEL_LAYOUT(pFMT->nChannels);
}

void AudioParameter::SetAudioFormat(IPC_AUDIO_FORMAT s)
{
	m_audioF = s;
	m_wPlanes = IsAudioPlanar(m_audioF) ? m_wChannels : 1;
}

IPC_AUDIO_FORMAT AudioParameter::GetAudioFormat() const
{
	return m_audioF;
}

void AudioParameter::SetChannelLayout(IPC_CHANNEL_LAYOUT s)
{
	m_channelLayout = s;
}

IPC_CHANNEL_LAYOUT AudioParameter::GetChannelLayout() const
{
	return m_channelLayout;
}
//////////////////////////////////////////////////////////////////////////
VideoParameter::VideoParameter()
	: m_format(IPC_PIXEL_FORMAT_NV12)
	, m_cs(IPC_COLOR_SPACE_BT601)
	, m_ct(IPC_COLOR_TRANSFER_SMPTE240M)
	, m_range(IPC_VIDEO_RANGE_FULL)
{

}

VideoParameter::~VideoParameter()
{
}

BOOL VideoParameter::operator!=(const VideoParameter& vps)
{
	if (vps.m_cs != m_cs)
		return TRUE;
	if (vps.m_ct != m_ct)
		return TRUE;
	if (vps.m_format != m_format)
		return TRUE;
	if (vps.m_range != m_range)
		return TRUE;
	return FALSE;
}

VideoParameter& VideoParameter::operator=(const VideoParameter& vps)
{
	if (this != &vps)
	{
		m_format = vps.m_format;
		m_cs = vps.m_cs;
		m_ct = vps.m_ct;
		m_range = vps.m_range;
	}
	return *this;
}

IPC_VIDEO_RANGE VideoParameter::GetRange() const
{
	return m_range;
}


IPC_VIDEO_PIXEL_FORMAT VideoParameter::GetFormat() const
{
	return m_format;
}

IPC_COLOR_SPACE VideoParameter::GetCS() const
{
	return m_cs;
}

IPC_COLOR_TRANSFER VideoParameter::GetCT() const
{
	return m_ct;
}

void VideoParameter::SetRange(IPC_VIDEO_RANGE vRange)
{
	m_range = vRange;
}
SIZE VideoParameter::GetSize() const
{
	return m_size;
}
void VideoParameter::SetSize(const SIZE& size)
{
	m_size = size;
}
void VideoParameter::SetFormat(IPC_VIDEO_PIXEL_FORMAT vFormat)
{
	m_format = vFormat;
}

void VideoParameter::SetCS(IPC_COLOR_SPACE cs)
{
	m_cs = cs;
	switch (cs)
	{
	case IPC_COLOR_SPACE_SMPTE240M:
		m_ct = IPC_COLOR_TRANSFER_SMPTE240M;
		break;
	case IPC_COLOR_SPACE_BT601:
		m_ct = IPC_COLOR_TRANSFER_SMPTE170M;
		break;
	case IPC_COLOR_SPACE_BT709:
		m_ct = IPC_COLOR_TRANSFER_BT709;
		break;
	case IPC_COLOR_SPACE_BT2020:
		m_ct = IPC_COLOR_TRANSFER_BT2020_10;
		break;
	case IPC_COLOR_SPACE_BT2100:
		m_ct = IPC_COLOR_TRANSFER_SMPTE2084;
		break;
	default:
		m_ct = IPC_COLOR_TRANSFER_BT709;
		break;
	}
}

void VideoParameter::SetCT(IPC_COLOR_TRANSFER ct)
{
	m_ct = ct;
}
