// src/filters/soloud_dattorro_filter.cpp
#include "soloud_dattorro_filter.h"

namespace SoLoud
{
    DattorroFilterInstance::DattorroFilterInstance(DattorroFilter *aParent)
    {
        mParent = aParent;
        mInited = false;

        initParams(5);

        mParam[DattorroFilter::PREDELAY] = mParent->mPreDelay;
        mParam[DattorroFilter::DECAY] = mParent->mDecay;
        mParam[DattorroFilter::DAMPING] = mParent->mDamping;
        mParam[DattorroFilter::WET] = mParent->mWet;
        mParam[DattorroFilter::DRY] = mParent->mDry;
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
        mPreDelay = 0.0f;
        mDecay = 0.7f;
        mDamping = 0.5f;
        mWet = 0.8f;
        mDry = 0.5f;
    }

    FilterInstance *DattorroFilter::createInstance()
    {
        return new DattorroFilterInstance(this);
    }

    int DattorroFilter::getParamCount() { return 5; }

    const char* DattorroFilter::getParamName(unsigned int aParamIndex) {
        switch (aParamIndex) {
            case PREDELAY: return "PreDelay";
            case DECAY: return "Decay";
            case DAMPING: return "Damping";
            case WET: return "Wet";
            case DRY: return "Dry";
        }
        return "N/A";
    }

    unsigned int DattorroFilter::getParamType(unsigned int aParamIndex) {
        return Filter::FLOAT_PARAM;
    }

    float DattorroFilter::getParamMax(unsigned int aParamIndex) {
        switch (aParamIndex) {
            case PREDELAY: return 1.0f;
            case DECAY: return 0.99f;
            case DAMPING: return 0.99f;
            case WET: return 1.0f;
            case DRY: return 1.0f;
        }
        return 1.0f;
    }

    float DattorroFilter::getParamMin(unsigned int aParamIndex) {
        return 0.0f;
    }
}
