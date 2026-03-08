// src/filters/soloud_convolutionfilter.h
#ifndef SOLOUD_CONVOLUTIONFILTER_H
#define SOLOUD_CONVOLUTIONFILTER_H

#include "soloud.h"
#include "fftconvolver/FFTConvolver.h"
#include <vector>

namespace SoLoud
{
    class ConvolutionFilter;

    class ConvolutionFilterInstance : public FilterInstance
    {
        ConvolutionFilter *mParent;
        fftconvolver::FFTConvolver mConvolverL;
        fftconvolver::FFTConvolver mConvolverR;
        
        // We need to keep a copy of the IR size or id to know if it was reloaded
        int mIrId;

        // Pre-allocated buffers to avoid heap allocation in the audio thread
        std::vector<float> mOutputL;
        std::vector<float> mOutputR;

    public:
        virtual void filter(float *aBuffer, unsigned int aSamples, unsigned int aBufferSize, unsigned int aChannels, float aSamplerate, time aTime);
        ConvolutionFilterInstance(ConvolutionFilter *aParent);
        virtual ~ConvolutionFilterInstance();
    };

    class ConvolutionFilter : public Filter
    {
    public:
        enum FILTERPARAMS {
            WET = 0,
            DRY
        };

        float mWet;
        float mDry;
        
        std::vector<float> mIrDataL;
        std::vector<float> mIrDataR;
        int mIrId;

        ConvolutionFilter();
        virtual ~ConvolutionFilter();

        // Load an impulse response file using SoLoud::Wav
        int loadIR(const char *aFilename);

        virtual int getParamCount();
        virtual const char* getParamName(unsigned int aParamIndex);
        virtual unsigned int getParamType(unsigned int aParamIndex);
        virtual float getParamMax(unsigned int aParamIndex);
        virtual float getParamMin(unsigned int aParamIndex);

        virtual FilterInstance *createInstance();
    };
}

#endif
