// src/filters/soloud_reverbsc.cpp
#include "soloud_reverbsc.h"

namespace SoLoud
{
    ReverbScInstance::ReverbScInstance(ReverbScFilter *aParent)
    {
        mParent = aParent;
        mInited = false;

        initParams(4);
        
        mParam[ReverbScFilter::FEEDBACK] = mParent->mFeedback;
        mParam[ReverbScFilter::LP_FREQ] = mParent->mLpFreq;
        mParam[ReverbScFilter::WET] = mParent->mWet;
        mParam[ReverbScFilter::DRY] = mParent->mDry;
    }

    ReverbScInstance::~ReverbScInstance()
    {
    }

    void ReverbScInstance::filter(
        float *aBuffer, 
        unsigned int aSamples, 
        unsigned int aBufferSize, 
        unsigned int aChannels, 
        float aSamplerate, 
        double aTime)
    {
        if (!mInited) {
            mReverb.init(aSamplerate);
            mInited = true;
        }

        updateParams(aTime);

        mReverb.setDecay(mParam[ReverbScFilter::FEEDBACK]);
        
        float damp = 1.0f - (mParam[ReverbScFilter::LP_FREQ] / 22000.0f);
        if (damp < 0.0f) damp = 0.0f;
        if (damp > 1.0f) damp = 1.0f;
        mReverb.setDamping(damp);

        mReverb.setWet(mParam[ReverbScFilter::WET]);
        mReverb.setDry(mParam[ReverbScFilter::DRY]);

        // SoLoud uses PLANAR buffer format (like PitchShift):
        // aBuffer[0 .. aSamples-1] = Channel 0 (Left)
        // aBuffer[aSamples .. 2*aSamples-1] = Channel 1 (Right)
        
        if (aChannels >= 2) {
            // Stereo: process with L/R
            float* bufL = &aBuffer[0];
            float* bufR = &aBuffer[aSamples];
            
            for (unsigned int i = 0; i < aSamples; i++) {
                float inL = bufL[i];
                float inR = bufR[i];
                float outL, outR;
                
                mReverb.processSample(inL, inR, outL, outR);
                
                bufL[i] = outL;
                bufR[i] = outR;
            }
        } else {
            // Mono: process L=R
            for (unsigned int i = 0; i < aSamples; i++) {
                float in = aBuffer[i];
                float outL, outR;
                
                mReverb.processSample(in, in, outL, outR);
                
                aBuffer[i] = (outL + outR) * 0.5f;
            }
        }
    }

    // --- Filter Class ---

    ReverbScFilter::ReverbScFilter()
    {
        mFeedback = 0.5f;
        mLpFreq = 10000.0f;
        mWet = 1.0f;
        mDry = 0.5f;
    }

    void ReverbScFilter::setParams(float aFeedback, float aLpFreq, float aWet, float aDry)
    {
        mFeedback = aFeedback;
        mLpFreq = aLpFreq;
        mWet = aWet;
        mDry = aDry;
    }

    FilterInstance *ReverbScFilter::createInstance()
    {
        return new ReverbScInstance(this);
    }

    int ReverbScFilter::getParamCount() { return 4; }
    
    const char* ReverbScFilter::getParamName(unsigned int aParamIndex) {
        switch (aParamIndex) {
            case FEEDBACK: return "Feedback";
            case LP_FREQ: return "LpFreq";
            case WET: return "Wet";
            case DRY: return "Dry";
        }
        return "N/A";
    }
    
    unsigned int ReverbScFilter::getParamType(unsigned int aParamIndex) { 
        return Filter::FLOAT_PARAM; 
    }
    
    float ReverbScFilter::getParamMax(unsigned int aParamIndex) { 
        if (aParamIndex == LP_FREQ) return 22000.0f;
        return 1.0f; 
    }
    
    float ReverbScFilter::getParamMin(unsigned int aParamIndex) { 
        return 0.0f; 
    }
}