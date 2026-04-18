// src/filters/soloud_dattorro_filter.h
#ifndef SOLOUD_DATTORRO_FILTER_H
#define SOLOUD_DATTORRO_FILTER_H

#include "soloud.h"
#include "dattorro.h"

namespace SoLoud
{
    class DattorroFilter;

    class DattorroFilterInstance : public FilterInstance
    {
        dattorro::DattorroReverb mReverb;
        DattorroFilter *mParent;
        bool mInited;

    public:
        virtual void filter(
            float *aBuffer,
            unsigned int aSamples,
            unsigned int aBufferSize,
            unsigned int aChannels,
            float aSamplerate,
            double aTime);

        virtual ~DattorroFilterInstance();
        DattorroFilterInstance(DattorroFilter *aParent);
    };

    class DattorroFilter : public Filter
    {
    public:
        enum FILTERPARAMS {
            PREDELAY = 0,
            DECAY,
            DAMPING,
            WET,
            DRY,
            BANDWIDTH,
            INPUT_DIFFUSION,
            LFO_RATE,
            LFO_DEPTH
        };

        float mPreDelay;
        float mDecay;
        float mDamping;
        float mWet;
        float mDry;
        float mBandwidth;
        float mInputDiffusion;
        float mLfoRate;
        float mLfoDepth;

        virtual FilterInstance *createInstance();
        DattorroFilter();

        virtual int getParamCount();
        virtual const char* getParamName(unsigned int aParamIndex);
        virtual unsigned int getParamType(unsigned int aParamIndex);
        virtual float getParamMax(unsigned int aParamIndex);
        virtual float getParamMin(unsigned int aParamIndex);
    };
}
#endif
