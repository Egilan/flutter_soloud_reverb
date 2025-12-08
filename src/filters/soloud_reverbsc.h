#ifndef SOLOUD_REVERBSC_H
#define SOLOUD_REVERBSC_H

#include "soloud.h"
#include "dattorro.h"

namespace SoLoud
{
    class ReverbScFilter;

    class ReverbScInstance : public FilterInstance
    {
        dattorro::DattorroReverb mReverb;
        ReverbScFilter *mParent;
        bool mInited; // <--- VITAL to prevent crash
        
    public:
        virtual void filter(
            float *aBuffer, 
            unsigned int aSamples, 
            unsigned int aBufferSize, 
            unsigned int aChannels, 
            float aSamplerate, 
            double aTime);

        virtual ~ReverbScInstance();
        ReverbScInstance(ReverbScFilter *aParent);
    };
    
    // ... (rest of file is same as before) ...
    class ReverbScFilter : public Filter
    {
    public:
        enum FILTERPARAMS {
            FEEDBACK = 0,
            LP_FREQ,
            WET,
            DRY
        };

        float mFeedback;
        float mLpFreq;
        float mWet;
        float mDry;

        virtual FilterInstance *createInstance();
        ReverbScFilter();
        
        void setParams(float aFeedback, float aLpFreq, float aWet = 1.0f, float aDry = 0.5f);
        
        virtual int getParamCount();
        virtual const char* getParamName(unsigned int aParamIndex);
        virtual unsigned int getParamType(unsigned int aParamIndex);
        virtual float getParamMax(unsigned int aParamIndex);
        virtual float getParamMin(unsigned int aParamIndex);
    };
}
#endif