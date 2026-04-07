#include "Resampler.h"

namespace IPCDemo
{
AudioResampler::AudioResampler()
    : m_context(NULL)
    , m_oSize(0)
    , m_oChannels(0)
    , m_iSamplesPerSec(0)
    , m_oSamplesPerSec(0)
    , m_iAVSFormat(AV_SAMPLE_FMT_NONE)
    , m_oAVSFormat(AV_SAMPLE_FMT_NONE)
{
    memset(&m_iChannelLayout, 0, sizeof(m_iChannelLayout));
    memset(&m_oChannelLayout, 0, sizeof(m_oChannelLayout));
    memset(&m_outputs, 0, sizeof(m_outputs));
}

AudioResampler::~AudioResampler()
{
    Close();
}

AudioResampler::AudioResampler(AudioResampler&& o) noexcept
    : m_context(o.m_context)
    , m_inputParamter(std::move(o.m_inputParamter))
    , m_outputParamter(std::move(o.m_outputParamter))
    , m_oSize(o.m_oSize)
    , m_oChannels(o.m_oChannels)
    , m_oPlanes(o.m_oPlanes)
    , m_iSamplesPerSec(o.m_iSamplesPerSec)
    , m_oSamplesPerSec(o.m_oSamplesPerSec)
    , m_iChannelLayout(o.m_iChannelLayout)
    , m_oChannelLayout(o.m_oChannelLayout)
    , m_iAVSFormat(o.m_iAVSFormat)
    , m_oAVSFormat(o.m_oAVSFormat)
{
    o.m_context = NULL;
    o.m_oSize = 0;
    o.m_oChannels = 0;
    o.m_oPlanes = 0;
    o.m_iSamplesPerSec = 0;
    o.m_oSamplesPerSec = 0;
    o.m_iAVSFormat = AV_SAMPLE_FMT_NONE;
    o.m_oAVSFormat = AV_SAMPLE_FMT_NONE;
    memset(&o.m_iChannelLayout, 0, sizeof(o.m_iChannelLayout));
    memset(&o.m_oChannelLayout, 0, sizeof(o.m_oChannelLayout));
    memset(&m_outputs, 0, sizeof(m_outputs));
    for (UINT32 i = 0; i < MAX_AV_PLANES; i++)
    {
        m_outputs[i] = o.m_outputs[i];
        o.m_outputs[i] = NULL;
    }
}

void AudioResampler::swap(AudioResampler& o) noexcept
{
    std::swap(m_context, o.m_context);
    std::swap(m_oSize, o.m_oSize);
    std::swap(m_oChannels, o.m_oChannels);
    std::swap(m_oPlanes, o.m_oPlanes);
    std::swap(m_iSamplesPerSec, o.m_iSamplesPerSec);
    std::swap(m_oSamplesPerSec, o.m_oSamplesPerSec);
    std::swap(m_iChannelLayout, o.m_iChannelLayout);
    std::swap(m_oChannelLayout, o.m_oChannelLayout);
    std::swap(m_iAVSFormat, o.m_iAVSFormat);
    std::swap(m_oAVSFormat, o.m_oAVSFormat);
    for (UINT32 i = 0; i < MAX_AV_PLANES; i++)
        std::swap(m_outputs[i], o.m_outputs[i]);
    m_inputParamter.swap(o.m_inputParamter);
    m_outputParamter.swap(o.m_outputParamter);
}

AudioResampler& AudioResampler::operator=(AudioResampler&& o) noexcept
{
    if (&o != this)
    {
        if (m_context != NULL)
            swr_free(&m_context);
        m_context = NULL;
        if (m_outputs[0] != NULL)
            av_freep(&m_outputs[0]);
        ZeroMemory(&m_outputs, sizeof(m_outputs));
        m_context = o.m_context;
        m_inputParamter = std::move(o.m_inputParamter);
        m_outputParamter = std::move(o.m_outputParamter);
        m_oSize = o.m_oSize;
        m_oChannels = o.m_oChannels;
        m_oPlanes = o.m_oPlanes;
        m_iSamplesPerSec = o.m_iSamplesPerSec;
        m_oSamplesPerSec = o.m_oSamplesPerSec;
        m_iChannelLayout = o.m_iChannelLayout;
        m_oChannelLayout = o.m_oChannelLayout;
        m_iAVSFormat = o.m_iAVSFormat;
        m_oAVSFormat = o.m_oAVSFormat;
        o.m_context = NULL;
        o.m_oSize = 0;
        o.m_oChannels = 0;
        o.m_oPlanes = 0;
        o.m_iSamplesPerSec = 0;
        o.m_oSamplesPerSec = 0;
        memset(&o.m_iChannelLayout, 0, sizeof(o.m_iChannelLayout));
        memset(&o.m_oChannelLayout, 0, sizeof(o.m_oChannelLayout));
        o.m_iAVSFormat = AV_SAMPLE_FMT_NONE;
        o.m_oAVSFormat = AV_SAMPLE_FMT_NONE;
        for (UINT32 i = 0; i < MAX_AV_PLANES; i++)
            m_outputs[i] = o.m_outputs[i];
        ZeroMemory(&o.m_outputs, sizeof(o.m_outputs));
    }
    return *this;
}

BOOL AudioResampler::IsEmpty() const noexcept
{
    return m_context == NULL;
}

const AudioParameter& AudioResampler::GetInputParameter() const noexcept
{
    return m_inputParamter;
}

const AudioParameter& AudioResampler::GetOutputParameter() const noexcept
{
    return m_outputParamter;
}

BOOL AudioResampler::Open(const AudioParameter& inputParamter, const AudioParameter& outputParamter)
{
    Close();
    if (inputParamter != outputParamter)
    {
        INT32 iRes = 0;
        m_iSamplesPerSec = (UINT32)inputParamter.GetSamplesPerSec();
        m_iAVSFormat = MS_AUDIO_FORMAT_TO_AV_SAMPLE_FMT(inputParamter.GetAudioFormat());
        av_channel_layout_default(&m_iChannelLayout, ChannelLayoutToChannels(inputParamter.GetChannelLayout()));
        m_oChannels = outputParamter.GetChannels();
        m_oSamplesPerSec = (UINT32)outputParamter.GetSamplesPerSec();
        av_channel_layout_default(&m_oChannelLayout, ChannelLayoutToChannels(outputParamter.GetChannelLayout()));
        m_oAVSFormat = MS_AUDIO_FORMAT_TO_AV_SAMPLE_FMT(outputParamter.GetAudioFormat());
        m_oPlanes = IsAudioPlanar(outputParamter.GetAudioFormat()) ? m_oChannels : 1;
        if (inputParamter.GetChannelLayout() == IPC_CHANNEL_3POINT1)
        {
            m_iChannelLayout.nb_channels = 4;
            m_iChannelLayout.order = AV_CHANNEL_ORDER_NATIVE;
            m_iChannelLayout.u.mask = AV_CH_LAYOUT_3POINT1;
        }
        if (inputParamter.GetChannelLayout() == IPC_CHANNEL_4POINT1)
        {
            m_iChannelLayout.nb_channels = 5;
            m_iChannelLayout.order = AV_CHANNEL_ORDER_NATIVE;
            m_iChannelLayout.u.mask = AV_CH_LAYOUT_4POINT1;
        }
        swr_alloc_set_opts2(&m_context,
                            &m_oChannelLayout,
                            m_oAVSFormat,
                            m_oSamplesPerSec,
                            &m_iChannelLayout,
                            m_iAVSFormat,
                            m_iSamplesPerSec,
                            0,
                            NULL);
        if (!m_context)
        {
            goto _ERROR;
        }
        AVChannelLayout monolayout;
        monolayout.nb_channels = 1;
        monolayout.order = AV_CHANNEL_ORDER_NATIVE;
        monolayout.u.mask = AV_CH_LAYOUT_MONO;
        if (av_channel_layout_compare(&m_iChannelLayout, &monolayout) == 0 && m_oChannels > 1)
        {
            const double matrix[MAX_AUDIO_CHANNELS][MAX_AUDIO_CHANNELS] = {
                {1},
                {1, 1},
                {1, 1, 0},
                {1, 1, 1, 1},
                {1, 1, 1, 0, 1},
                {1, 1, 1, 1, 1, 1},
                {1, 1, 1, 0, 1, 1, 1},
                {1, 1, 1, 0, 1, 1, 1, 1},
            };
            if (swr_set_matrix(m_context, matrix[m_oChannels - 1], 1) < 0)
            {
            }
        }
        iRes = swr_init(m_context);
        if (iRes != 0)
        {
            goto _ERROR;
        }
    }
    m_inputParamter = inputParamter;
    m_outputParamter = outputParamter;
    return TRUE;
_ERROR:
    Close();
    return FALSE;
}

BOOL AudioResampler::Resample(UINT8* output[], UINT32* oFrames, UINT64* delay, const UINT8* const input[],
                              UINT32 iFrames)
{
    if (!m_context)
        return FALSE;
    INT   iRes = 0;
    INT64 samples = swr_get_delay(m_context, m_iSamplesPerSec);
    INT32 estimate =
        (INT32)av_rescale_rnd(samples + (INT64)iFrames, (INT64)m_oSamplesPerSec, (INT64)m_iSamplesPerSec, AV_ROUND_UP);
    *delay = (UINT64)swr_get_delay(m_context, 1000000000);
    if (estimate > m_oSize)
    {
        if (m_outputs[0] != NULL)
            av_freep(&m_outputs[0]);
        av_samples_alloc(m_outputs, NULL, m_oChannels, estimate, m_oAVSFormat, 0);
        m_oSize = estimate;
    }
    iRes = swr_convert(m_context, m_outputs, m_oSize, (const UINT8**)input, iFrames);
    if (iRes < 0)
    {
        return FALSE;
    }
    for (UINT32 i = 0; i < m_oPlanes; i++)
        output[i] = m_outputs[i];
    *oFrames = (UINT32)iRes;
    return TRUE;
}

void AudioResampler::Close()
{
    if (m_context != NULL)
        swr_free(&m_context);
    m_context = NULL;
    if (m_outputs[0] != NULL)
        av_freep(&m_outputs[0]);
    ZeroMemory(&m_outputs, sizeof(m_outputs));
    m_oSize = 0;
    m_oChannels = 0;
    m_iSamplesPerSec = 0;
    m_oSamplesPerSec = 0;
    memset(&m_iChannelLayout, 0, sizeof(m_iChannelLayout));
    memset(&m_oChannelLayout, 0, sizeof(m_oChannelLayout));
    m_iAVSFormat = AV_SAMPLE_FMT_NONE;
    m_oAVSFormat = AV_SAMPLE_FMT_NONE;
}
} // namespace MediaSDK
