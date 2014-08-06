#pragma once

#include "OBSApi.h"

class NoiseReduction;

typedef void (*PFNFILTER)(float *, UINT n);

class IAudioBuffer
{

public:

    enum E_BUFFER_TYPE {
        MONO_BUFFER,
        STEREO_BUFFER
    };

    virtual E_BUFFER_TYPE GetType() const = 0;
    virtual void SetSize(UINT bufferSize) = 0;
    virtual UINT GetSize() const = 0;
    virtual UINT GetChannelCount() const = 0;
    virtual UINT InsertData(const float * src, UINT size) = 0;
    virtual UINT ExtractData(float * dst, UINT size) = 0;
    virtual void Reset() = 0;
    virtual bool CopyTo(IAudioBuffer* other) = 0;
    virtual const float * GetRawData() const = 0;
    virtual void ApplyFilter(PFNFILTER f) = 0;
};

class StereoAudioBuffer : public IAudioBuffer
{
public:

    StereoAudioBuffer() 
    : mWriteOffset(0)
    , mReadOffset(0)
    , mSize(0)
    , mData(nullptr)
    {
        mChannel[0] = mChannel[1] = nullptr;
    }
    
    virtual ~StereoAudioBuffer() {
        delete[] mData;
    }

    virtual E_BUFFER_TYPE GetType() const {
        return STEREO_BUFFER;
    }

    virtual void SetSize(UINT size) override {
        if (size == mSize)
            return;

        Free();

        mData = new float[2 * size];
        mChannel[0] = mData;
        mChannel[1] = mData + size;
        mSize = size;
    }
    virtual UINT GetSize() const override { return mSize; }
    virtual UINT GetChannelCount() const override { return 2; }
    virtual void Reset() override { mWriteOffset = mReadOffset = 0; }

    virtual UINT InsertData(const float * src, UINT size) override {
        if (!src)
            return 0;

        UINT freeCapacity = GetFreeCapacity();
        UINT samplesToRead = (2 * freeCapacity < size) ? 2 * freeCapacity : size;
        UINT idx = mWriteOffset;
        for (UINT i = 0; i < samplesToRead; i += 2) {
            mChannel[0][idx] = src[i];
            mChannel[1][idx] = src[i + 1];
            idx++;
        }

        mWriteOffset = idx;

        return samplesToRead;
    }

    virtual UINT ExtractData(float * dst, UINT size) override {
        if (!dst)
            return 0;

        UINT numElemsLeft = GetNumElemsToRead();
        UINT samplesToWrite = (2 * numElemsLeft < size) ? 2 * numElemsLeft : size;
        UINT idx = mReadOffset;
        for (UINT i = 0; i < samplesToWrite; i += 2) {
            dst[i] = mChannel[0][idx];
            dst[i + 1] = mChannel[1][idx];
            idx++;
        }

        mReadOffset = idx;

        return samplesToWrite;
    }

    virtual bool CopyTo(IAudioBuffer * other) override {

        if (!other)
            return false;
        else if (other->GetType() != this->GetType())
            return false;

        other->SetSize(mSize);

        StereoAudioBuffer * ref = (StereoAudioBuffer *)other;
        memcpy(ref->mData, this->mData, 2 * mSize*sizeof(float));
        return true;
    }

    virtual const float * GetRawData() const {
        return mData;
    }

    virtual void ApplyFilter(PFNFILTER f) {
        f(mChannel[0], mSize);
        f(mChannel[1], mSize);
    }

    void Free() {
        mWriteOffset = 0;
        mReadOffset = 0;
        mSize = 0;
        
        if (mData)
        {
            delete[] mData;
            mData = nullptr;
        }
        
        mChannel[0] = mChannel[1] = nullptr;
    }

    const float * GetLeftChannel() const {
        return mChannel[0];
    }

    const float * GetChannelRight() const {
        return mChannel[1];
    }

    UINT GetFreeCapacity() const {
        return mSize - mWriteOffset;
    }

    UINT GetNumElemsToRead() const {
        return mSize - mReadOffset;
    }

private:

    UINT    mWriteOffset;
    UINT    mReadOffset;
    UINT    mSize;
    float * mData;
    float * mChannel[2];

};

class SwapChain
{
    public:

        SwapChain(IAudioBuffer * writeBuffer, IAudioBuffer * readBuffer1, IAudioBuffer * readBuffer2) {
            mBuffers[0] = writeBuffer;
            mBuffers[1] = readBuffer1;
            mBuffers[2] = readBuffer2;
            mWriteBuffer = 0;
            mReadBuffer = 1;
        }

        ~SwapChain() {
            for (int i = 0; i < 3; i++) {
                if (mBuffers[i])
                    delete mBuffers[i];
            }
        }

        IAudioBuffer * GetWriteBuffer() {
            return mBuffers[mWriteBuffer];
        }

        IAudioBuffer * GetReadBuffer() {
            return mBuffers[mReadBuffer];
        }

        void Swap() {
            GetReadBuffer()->Reset();
            mWriteBuffer = (mWriteBuffer + 1) % 3;
            mReadBuffer = (mReadBuffer + 1) % 3;
        }

        SwapChain(const SwapChain& other) = delete;

    private:

        IAudioBuffer * mBuffers[3];
        UINT mWriteBuffer;
        UINT mReadBuffer;
};

class NoiseReductionFilter : public AudioFilter
{
public:
    NoiseReductionFilter(NoiseReduction * p);
    virtual ~NoiseReductionFilter();

    virtual AudioSegment *  Process(AudioSegment * segment);
    virtual bool            Initialize(UINT bufferSize);
    virtual void            Free();

private:
    void Setup(UINT channelSize);
    void SetupMono();
    void SetupStereo();
    void EnqueAudioData(const AudioSegment * segment);
    void Flush();
    void FillAudioSegment(AudioSegment * outSeg);

    NoiseReduction *    parent;
    UINT                mBufferSize;
    QWORD               mLastTimestamp;
    bool                mbInitialized;

    IAudioBuffer *      mInputBuffer;
    SwapChain *         mSwapChain;

    enum {
        CHANNEL_MONO,
        CHANNEL_STEREO,
        CHANNEL_UNDEFINED
    } mChannelDesc;
};