#include "NoiseReductionFilter.h"
#include "NoiseReduction.h"
#include <fftw3.h>

#define SAFE_DELETE(x) if(x) { delete x; x = NULL; }

static bool is_power_of_two(int n)
{
    if (n < 2)
        return false;

    int m = 2;
    while (m < n && m > 1)
    {
        m *= 2;
    }

    return m == n;
}

NoiseReductionFilter::NoiseReductionFilter(NoiseReduction* p)
: parent(p)
, mLastTimestamp(0)
, mbInitialized(false)
, mChannelDesc(CHANNEL_UNDEFINED)
, mInputBuffer(nullptr)
, mSwapChain(nullptr)
{
}

NoiseReductionFilter::~NoiseReductionFilter()
{
    Free();
}

AudioSegment * NoiseReductionFilter::Process(AudioSegment *segment)
{
    if (parent->IsEnabled() && mbInitialized)
    {
        EnqueAudioData(segment);
        FillAudioSegment(segment);
    }

    return segment;
}

bool NoiseReductionFilter::Initialize(UINT bufferSize)
{
    static const char * init_fail_str = "Error initializing NoiseReductionFilter";

    Free();

    if (!parent)
    {
        nr_log(init_fail_str, "parent is NULL");
        return false;
    }
    
    const AudioSource * mic = parent->GetMicSource();
    if (!mic)
    {
        nr_log(init_fail_str, "no microphone source found");
        return false;
    }

    switch (mic->GetChannelCount())
    {
    case 1: 
        mChannelDesc = CHANNEL_MONO;
        break;
    case 2:
        mChannelDesc = CHANNEL_STEREO;
        break;
    default:
        nr_log(init_fail_str, "unhandled channel count (must be mono or stereo)");
        return false;
    }

    Setup(bufferSize);

    return mbInitialized;
}

void NoiseReductionFilter::Free()
{
    mChannelDesc = CHANNEL_UNDEFINED;
    mBufferSize = 0;
    mbInitialized = false;

    SAFE_DELETE(mInputBuffer);
    SAFE_DELETE(mSwapChain);
}

void NoiseReductionFilter::Setup(UINT bufferSize)
{
    mBufferSize = bufferSize;

    switch (mChannelDesc)
    {
    case CHANNEL_MONO:
        //SetupMono();
        //break;
        nr_log("Error in Setup", "mono channel handling not implemented");
        return;
    case CHANNEL_STEREO:
        SetupStereo();
        break;
    default:
        nr_log("Error in Setup", "undefined channel count");
        return;
    }

    nr_log("Setup", "completed");

    mbInitialized = true;
}

void NoiseReductionFilter::SetupMono()
{
    nr_log("SetupMono", "completed");
}

void NoiseReductionFilter::SetupStereo()
{
    mInputBuffer = new StereoAudioBuffer;
    mInputBuffer->SetSize(mBufferSize);

    StereoAudioBuffer * buffers[3];
    for (int i = 0; i < 3; i++) {
        buffers[i] = new StereoAudioBuffer;
        buffers[i]->SetSize(mBufferSize);
    }

    mSwapChain = new SwapChain(buffers[0], buffers[1], buffers[2]);

    nr_log("SetupStereo", "completed");
}

void NoiseReductionFilter::EnqueAudioData(const AudioSegment * segment)
{
    UINT segmentSize = segment->audioData.Num();
    UINT numElemsRead = mInputBuffer->InsertData(segment->audioData.Array(), segmentSize);
    if (numElemsRead != segmentSize)
    {
        Flush();
        mInputBuffer->Reset();
        mInputBuffer->InsertData(segment->audioData.Array() + numElemsRead, segmentSize - numElemsRead);
    }
}

static void noise_filter(float * inout, UINT n)
{
    fftwf_complex * buffer = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)* (n / 2 + 1));
    fftwf_plan to_freq_plan, to_time_plan;

    to_freq_plan = fftwf_plan_dft_r2c_1d(n, inout, buffer, FFTW_ESTIMATE);
    to_time_plan = fftwf_plan_dft_c2r_1d(n, buffer, inout, FFTW_ESTIMATE);

    fftwf_execute(to_freq_plan);

    // TODO: actual filtering

    fftwf_execute(to_time_plan);

    // normalize output
    const float norm = 1.f / n;
    for (UINT i = 0; i < n; i++)
    {
        inout[i] *= norm;
    }

    fftwf_destroy_plan(to_freq_plan);
    fftwf_destroy_plan(to_time_plan);
    fftwf_free(buffer);
}

void NoiseReductionFilter::Flush()
{
    mInputBuffer->CopyTo(mSwapChain->GetWriteBuffer());
    mSwapChain->GetWriteBuffer()->ApplyFilter(noise_filter);
}

void NoiseReductionFilter::FillAudioSegment(AudioSegment * segOut)
{
    if (!segOut)
        return;

    UINT segmentSize = segOut->audioData.Num();
    UINT elemsRead = mSwapChain->GetReadBuffer()->ExtractData(segOut->audioData.Array(), segmentSize);
    if (elemsRead != segmentSize)
    {
        mSwapChain->GetReadBuffer();
        mSwapChain->Swap();
        mSwapChain->GetReadBuffer()->ExtractData(segOut->audioData.Array() + elemsRead, segmentSize - elemsRead);
    }
}