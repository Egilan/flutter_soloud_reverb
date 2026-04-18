// src/filters/soloud_dattorro_filter.cpp
#include "soloud_dattorro_filter.h"

namespace SoLoud
{
    DattorroFilterInstance::DattorroFilterInstance(DattorroFilter *aParent)
    {
        mParent = aParent;
        mInited = false;

        initParams(9);

        mParam[DattorroFilter::PREDELAY]        = mParent->mPreDelay;
        mParam[DattorroFilter::DECAY]           = mParent->mDecay;
        mParam[DattorroFilter::DAMPING]         = mParent->mDamping;
        mParam[DattorroFilter::WET]             = mParent->mWet;
        mParam[DattorroFilter::DRY]             = mParent->mDry;
        mParam[DattorroFilter::BANDWIDTH]       = mParent->mBandwidth;
        mParam[DattorroFilter::INPUT_DIFFUSION] = mParent->mInputDiffusion;
        mParam[DattorroFilter::LFO_RATE]        = mParent->mLfoRate;
        mParam[DattorroFilter::LFO_DEPTH]       = mParent->mLfoDepth;
    }

    DattorroFilterInstance::~DattorroFilterInstance()
    {
    }

    void DattorroFilterInstance::filter(
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

        mReverb.setPreDelay(mParam[DattorroFilter::PREDELAY]);
        mReverb.setDecay(mParam[DattorroFilter::DECAY]);
        mReverb.setDamping(mParam[DattorroFilter::DAMPING]);
        mReverb.setWet(mParam[DattorroFilter::WET]);
        mReverb.setDry(mParam[DattorroFilter::DRY]);
        mReverb.setBandwidth(mParam[DattorroFilter::BANDWIDTH]);
        mReverb.setInputDiffusion(mParam[DattorroFilter::INPUT_DIFFUSION]);
        mReverb.setLfoRate(mParam[DattorroFilter::LFO_RATE]);
        mReverb.setLfoDepth(mParam[DattorroFilter::LFO_DEPTH]);

        if (aChannels >= 2) {
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
            for (unsigned int i = 0; i < aSamples; i++) {
                float in = aBuffer[i];
                float outL, outR;

                mReverb.processSample(in, in, outL, outR);

                aBuffer[i] = (outL + outR) * 0.5f;
            }
        }
    }

    // --- Filter Class ---

    DattorroFilter::DattorroFilter()
    {
        mPreDelay       = 0.0f;
        mDecay          = 0.7f;
        mDamping        = 0.5f;
        mWet            = 0.8f;
        mDry            = 0.5f;
        mBandwidth      = 1.0f;   // fully transparent by default
        mInputDiffusion = 1.0f;   // maximum diffusion by default
        mLfoRate        = 1.0f;   // 1 Hz (Dattorro paper default)
        mLfoDepth       = 1.0f;   // full excursion by default
    }

    FilterInstance *DattorroFilter::createInstance()
    {
        return new DattorroFilterInstance(this);
    }

    int DattorroFilter::getParamCount() { return 9; }

    const char* DattorroFilter::getParamName(unsigned int aParamIndex) {
        switch (aParamIndex) {
            case PREDELAY:        return "PreDelay";
            case DECAY:           return "Decay";
            case DAMPING:         return "Damping";
            case WET:             return "Wet";
            case DRY:             return "Dry";
            case BANDWIDTH:       return "Bandwidth";
            case INPUT_DIFFUSION: return "InputDiffusion";
            case LFO_RATE:        return "LfoRate";
            case LFO_DEPTH:       return "LfoDepth";
        }
        return "N/A";
    }

    unsigned int DattorroFilter::getParamType(unsigned int aParamIndex) {
        return Filter::FLOAT_PARAM;
    }

    float DattorroFilter::getParamMax(unsigned int aParamIndex) {
        switch (aParamIndex) {
            case PREDELAY:        return 1.0f;
            case DECAY:           return 0.99f;
            case DAMPING:         return 0.99f;
            case WET:             return 1.0f;
            case DRY:             return 1.0f;
            case BANDWIDTH:       return 1.0f;
            case INPUT_DIFFUSION: return 1.0f;
            case LFO_RATE:        return 10.0f;  // Hz
            case LFO_DEPTH:       return 1.0f;
        }
        return 1.0f;
    }

    float DattorroFilter::getParamMin(unsigned int aParamIndex) {
        if (aParamIndex == LFO_RATE) return 0.1f;
        return 0.0f;
    }
}
