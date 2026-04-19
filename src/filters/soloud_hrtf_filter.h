// src/filters/soloud_hrtf_filter.h
#ifndef SOLOUD_HRTF_FILTER_H
#define SOLOUD_HRTF_FILTER_H

#include "soloud.h"
#include <array>
#include <vector>
#include <atomic>

namespace SoLoud
{
    class HrtfFilter;

    static const int HRTF_IR_SIZE = 200;

    struct HrirEntry {
        float azimuth;    // degrees
        float elevation;  // degrees
        std::array<float, HRTF_IR_SIZE> left;
        std::array<float, HRTF_IR_SIZE> right;
    };

    class HrtfFilterInstance : public FilterInstance
    {
        HrtfFilter *mParent;
        bool mInited;
        float mSamplerate;

        // Ring buffer for convolution history (hi-pass signal)
        std::array<float, HRTF_IR_SIZE> mHistory;
        int mHistPos;

        // Current and target HRIRs for crossfade
        std::array<float, HRTF_IR_SIZE> mCurrentIrL, mCurrentIrR;
        std::array<float, HRTF_IR_SIZE> mTargetIrL, mTargetIrR;
        // Working IR (blended) to avoid per-sample stack allocation
        std::array<float, HRTF_IR_SIZE> mWorkIrL, mWorkIrR;

        float mLastAzimuth, mLastElevation;
        int mCrossfadePos;
        static const int CROSSFADE_SAMPLES = 512;

        // Biquad crossover state
        struct BiquadState { float x1, x2, y1, y2; };
        struct BiquadCoeffs { float b0, b1, b2, a1, a2; };
        BiquadState mLoState, mHiState;
        BiquadCoeffs mLoCoeffs, mHiCoeffs;
        float mLastCrossoverFreq;

        void updateCrossover(float freq, float samplerate);
        float processBiquad(float x, BiquadState &s, const BiquadCoeffs &c);
        void blendIr(float blend);

    public:
        void filter(
            float *aBuffer,
            unsigned int aSamples,
            unsigned int aBufferSize,
            unsigned int aChannels,
            float aSamplerate,
            double aTime) override;

        bool expandsToStereo() const override { return true; }

        HrtfFilterInstance(HrtfFilter *aParent);
        ~HrtfFilterInstance() override;
    };

    class HrtfFilter : public Filter
    {
    public:
        enum FILTERPARAMS {
            AZIMUTH = 0,
            ELEVATION,
            WET,
            CROSSOVER_FREQ
        };

        float mAzimuth;
        float mElevation;
        float mWet;
        float mCrossoverFreq;

        std::vector<HrirEntry> mHrirData;
        std::atomic<bool> mDataReady;

        HrtfFilter();
        ~HrtfFilter() override;

        // Load KEMAR binary file (27 azimuths × 52 elevations × 200+200 floats).
        // Returns 0 on success, negative on error.
        int loadKemarBinary(const char *path);

        // Interpolate an HRIR for the given azimuth/elevation using
        // inverse-distance weighting over the 3 nearest measurements.
        void interpolate(float azimuth, float elevation,
                         float *irL, float *irR) const;

        FilterInstance *createInstance() override;
        int getParamCount() override;
        const char *getParamName(unsigned int aParamIndex) override;
        unsigned int getParamType(unsigned int aParamIndex) override;
        float getParamMax(unsigned int aParamIndex) override;
        float getParamMin(unsigned int aParamIndex) override;
    };

} // namespace SoLoud

#endif // SOLOUD_HRTF_FILTER_H
