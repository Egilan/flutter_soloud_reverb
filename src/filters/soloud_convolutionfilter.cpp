// src/filters/soloud_convolutionfilter.cpp
#include "soloud_convolutionfilter.h"
#include "soloud_wav.h"

namespace SoLoud
{
    ConvolutionFilterInstance::ConvolutionFilterInstance(ConvolutionFilter *aParent)
    {
        mParent = aParent;
        mIrId = -1;
        initParams(2);
        mParam[0] = aParent->mWet;
        mParam[1] = aParent->mDry;
    }

    ConvolutionFilterInstance::~ConvolutionFilterInstance()
    {
    }

    void ConvolutionFilterInstance::filter(
        float *aBuffer, 
        unsigned int aSamples, 
        unsigned int aBufferSize, 
        unsigned int aChannels, 
        float aSamplerate, 
        time aTime)
    {
        updateParams(aTime);

        // Check if IR was updated
        if (mIrId != mParent->mIrId) {
            mIrId = mParent->mIrId;
            if (mParent->mIrDataL.size() > 0) {
                mConvolverL.init(1024, mParent->mIrDataL.data(), mParent->mIrDataL.size());
            } else {
                mConvolverL.reset();
            }
            if (mParent->mIrDataR.size() > 0) {
                mConvolverR.init(1024, mParent->mIrDataR.data(), mParent->mIrDataR.size());
            } else {
                mConvolverR.reset();
            }
        }

        float wet = mParam[0];
        float dry = mParam[1];

        if (aChannels >= 2) {
            float* bufL = &aBuffer[0];
            float* bufR = &aBuffer[aBufferSize];
            
            // Process Left
            if (mParent->mIrDataL.size() > 0) {
                if (mOutputL.size() < aSamples) mOutputL.resize(aSamples);
                mConvolverL.process(bufL, mOutputL.data(), aSamples);
                for (unsigned int i = 0; i < aSamples; ++i) {
                    bufL[i] = bufL[i] * dry + mOutputL[i] * wet;
                }
            }
            
            // Process Right
            if (mParent->mIrDataR.size() > 0) {
                if (mOutputR.size() < aSamples) mOutputR.resize(aSamples);
                mConvolverR.process(bufR, mOutputR.data(), aSamples);
                for (unsigned int i = 0; i < aSamples; ++i) {
                    bufR[i] = bufR[i] * dry + mOutputR[i] * wet;
                }
            }
        } else {
            // Mono
            if (mParent->mIrDataL.size() > 0) {
                if (mOutputL.size() < aSamples) mOutputL.resize(aSamples);
                mConvolverL.process(aBuffer, mOutputL.data(), aSamples);
                for (unsigned int i = 0; i < aSamples; ++i) {
                    aBuffer[i] = aBuffer[i] * dry + mOutputL[i] * wet;
                }
            }
        }
    }

    ConvolutionFilter::ConvolutionFilter()
    {
        mWet = 1.0f;
        mDry = 0.5f;
        mIrId = 0;
    }

    ConvolutionFilter::~ConvolutionFilter()
    {
    }

    int ConvolutionFilter::loadIR(const char *aFilename)
    {
        SoLoud::Wav wav;
        result res = wav.load(aFilename);
        if (res != SO_NO_ERROR) return res;

        // Extract float data from wav
        // SoLoud::Wav loads into floats internally
        // Wav internal representation is interleaved or planar? 
        // As of SoLoud::Wav, audio data is float. But we can just use the AudioSource method GetAudio.
        // But the easiest way without modifying SoLoud::Wav to expose its internal buffer is to read it using the AudioSourceInstance.

        SoLoud::AudioSourceInstance* instance = wav.createInstance();
        if (instance == nullptr) return UNKNOWN_ERROR;

        unsigned int samples = wav.mSampleCount;
        unsigned int channels = wav.mChannels;
        
        mIrDataL.resize(samples);
        mIrDataR.resize(samples);

        float peak = 0.0001f; // Avoid division by zero

        // Read data directly from planar Wav buffers
        for (unsigned int i = 0; i < samples; ++i) {
            mIrDataL[i] = wav.mData[i];
            float absL = fabsf(mIrDataL[i]);
            if (absL > peak) peak = absL;

            if (channels >= 2) {
                mIrDataR[i] = wav.mData[samples + i];
                float absR = fabsf(mIrDataR[i]);
                if (absR > peak) peak = absR;
            } else {
                mIrDataR[i] = mIrDataL[i];
            }
        }
        
        // Peak normalization
        for (unsigned int i = 0; i < samples; ++i) {
            mIrDataL[i] /= peak;
            mIrDataR[i] /= peak;
        }

        mIrId++; // Trigger convolvers to reload
        return SO_NO_ERROR;
    }

    int ConvolutionFilter::getParamCount() { return 2; }

    const char* ConvolutionFilter::getParamName(unsigned int aParamIndex) {
        if (aParamIndex == 0) return "Wet";
        if (aParamIndex == 1) return "Dry";
        return "N/A";
    }

    unsigned int ConvolutionFilter::getParamType(unsigned int aParamIndex) {
        return Filter::FLOAT_PARAM;
    }

    float ConvolutionFilter::getParamMax(unsigned int aParamIndex) {
        return 1.0f;
    }

    float ConvolutionFilter::getParamMin(unsigned int aParamIndex) {
        return 0.0f;
    }

    FilterInstance *ConvolutionFilter::createInstance()
    {
        return new ConvolutionFilterInstance(this);
    }
}
