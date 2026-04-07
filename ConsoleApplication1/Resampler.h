#pragma once
#include "Common.h"

namespace IPCDemo
{
class AudioResampler
{
public:
    AudioResampler();
    ~AudioResampler();
    AudioResampler(AudioResampler&& o) noexcept;
    AudioResampler& operator=(AudioResampler&& o) noexcept;
    void            swap(AudioResampler& o) noexcept;

public:
    BOOL                  IsEmpty() const noexcept;
    const AudioParameter& GetInputParameter() const noexcept;
    const AudioParameter& GetOutputParameter() const noexcept;
    BOOL                  Open(const AudioParameter& inputParamter, const AudioParameter& outputParamter);
    BOOL                  Resample(UINT8* output[], UINT32* oFrames, UINT64* delay, const UINT8* const input[], UINT32 iFrames);
    void                  Close();

private:
    SwrContext*     m_context;
    AudioParameter  m_inputParamter;
    AudioParameter  m_outputParamter;
    INT32           m_oSize;
    INT32           m_oChannels;
    UINT32          m_oPlanes;
    UINT32          m_iSamplesPerSec;
    UINT32          m_oSamplesPerSec;
    AVChannelLayout m_iChannelLayout;
    AVChannelLayout m_oChannelLayout;
    AVSampleFormat  m_iAVSFormat;
    AVSampleFormat  m_oAVSFormat;
    UINT8*          m_outputs[MAX_AV_PLANES];
};
} // namespace MediaSDK
