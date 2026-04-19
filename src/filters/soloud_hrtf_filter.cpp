// src/filters/soloud_hrtf_filter.cpp
//
// HRTF spatialization filter using direct time-domain convolution.
//
// Data format: KEMAR binary (MIT Media Lab).
//   Layout: 27 azimuths × 52 elevations × (200 left + 200 right) float32
//   Azimuths: [-90,-80,-65,-55,-45,-40,-35,-30,-25,-20,-15,-10,-5,
//               0,5,10,15,20,25,30,35,40,45,55,65,80,90] degrees
//   Elevations per azimuth: -90, then -45…+230.625 in 5.625° steps, then 270
//
// DSP chain per sample:
//   mono = downmix(L, R)
//   [biquad crossover at ~150 Hz]
//   loPass  -> added directly to both output channels (bass is non-directional)
//   hiPass  -> convolved with interpolated left/right HRIR
//   outL = wet * (loPass + convolvedL) + (1-wet) * mono
//   outR = wet * (loPass + convolvedR) + (1-wet) * mono
//
// Position changes crossfade over CROSSFADE_SAMPLES to prevent clicks.
// Biquad crossover from Audio EQ Cookbook (public domain, R. Bristow-Johnson).

#include "soloud_hrtf_filter.h"
#include <cmath>
#include <fstream>
#include <algorithm>
#include <cstring>

#if defined(__ANDROID__)
  #include <android/log.h>
  #define HRTF_LOG(...) __android_log_print(ANDROID_LOG_INFO, "HRTF", __VA_ARGS__)
#else
  #include <cstdio>
  #define HRTF_LOG(...) do { fprintf(stderr, "[HRTF] "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while(0)
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SoLoud
{

// --------------------------------------------------------------------------
// KEMAR dataset constants
// --------------------------------------------------------------------------

static const int KEMAR_NUM_AZIMUTHS   = 27;
static const int KEMAR_NUM_ELEVATIONS = 52;

static const int KEMAR_AZIMUTHS[KEMAR_NUM_AZIMUTHS] = {
    -90, -80, -65, -55, -45, -40, -35, -30, -25, -20, -15, -10, -5, 0,
    5, 10, 15, 20, 25, 30, 35, 40, 45, 55, 65, 80, 90
};

// Elevation index (0-51) to degrees
static float kemarElevationDeg(int idx)
{
    if (idx == 0)  return -90.0f;
    if (idx == 51) return 270.0f;
    return -45.0f + (float)(idx - 1) * 5.625f;
}

// --------------------------------------------------------------------------
// HrtfFilter
// --------------------------------------------------------------------------

HrtfFilter::HrtfFilter()
    : mAzimuth(0.0f), mElevation(0.0f), mWet(1.0f), mCrossoverFreq(150.0f),
      mDataReady(false)
{
}

HrtfFilter::~HrtfFilter() {}

int HrtfFilter::loadKemarBinary(const char *path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open())
        return -1;

    std::vector<HrirEntry> data;
    data.reserve(KEMAR_NUM_AZIMUTHS * KEMAR_NUM_ELEVATIONS);

    for (int a = 0; a < KEMAR_NUM_AZIMUTHS; a++) {
        for (int e = 0; e < KEMAR_NUM_ELEVATIONS; e++) {
            HrirEntry entry;
            entry.azimuth   = (float)KEMAR_AZIMUTHS[a];
            entry.elevation = kemarElevationDeg(e);

            f.read(reinterpret_cast<char *>(entry.left.data()),
                   HRTF_IR_SIZE * sizeof(float));
            f.read(reinterpret_cast<char *>(entry.right.data()),
                   HRTF_IR_SIZE * sizeof(float));

            if (!f)
                return -2;

            // Normalize so the louder ear has unit peak, preserving ITD/ILD ratio.
            float peak = 0.0f;
            for (int k = 0; k < HRTF_IR_SIZE; k++) {
                float al = fabsf(entry.left[k]);
                float ar = fabsf(entry.right[k]);
                if (al > peak) peak = al;
                if (ar > peak) peak = ar;
            }
            if (peak > 1e-9f) {
                for (int k = 0; k < HRTF_IR_SIZE; k++) {
                    entry.left[k]  /= peak;
                    entry.right[k] /= peak;
                }
            }

            data.push_back(std::move(entry));
        }
    }

    // Publish data before setting the flag (memory_order_release ensures
    // the audio thread sees the complete write when it reads mDataReady).
    mHrirData = std::move(data);
    mDataReady.store(true, std::memory_order_release);
    return 0;
}

// Angular distance in degrees between two directions on the unit sphere.
// Uses KEMAR interaural polar coordinates: x=sin(az), y=cos(az)*cos(el), z=cos(az)*sin(el)
// where az=0,el=0 is directly forward; az=±90 is right/left; el=±90 is above/below.
// This avoids the standard-spherical pole degeneracy where all (az, ±90°) collapse to one point.
static float angularDistDeg(float az1, float el1, float az2, float el2)
{
    const float deg2rad = (float)M_PI / 180.0f;
    float a1 = az1 * deg2rad, e1 = el1 * deg2rad;
    float a2 = az2 * deg2rad, e2 = el2 * deg2rad;

    float x1 = sinf(a1), y1 = cosf(a1) * cosf(e1), z1 = cosf(a1) * sinf(e1);
    float x2 = sinf(a2), y2 = cosf(a2) * cosf(e2), z2 = cosf(a2) * sinf(e2);

    float dot = x1*x2 + y1*y2 + z1*z2;
    dot = (dot < -1.0f) ? -1.0f : (dot > 1.0f ? 1.0f : dot);
    return acosf(dot) * 180.0f / (float)M_PI;
}

void HrtfFilter::interpolate(float azimuth, float elevation,
                              float *irL, float *irR) const
{
    if (mHrirData.empty()) {
        // Identity (dirac) passthrough
        memset(irL, 0, HRTF_IR_SIZE * sizeof(float));
        memset(irR, 0, HRTF_IR_SIZE * sizeof(float));
        irL[0] = irR[0] = 1.0f;
        return;
    }

    // Clamp to practical range
    azimuth   = (azimuth   < -90.0f) ? -90.0f : (azimuth   > 90.0f ? 90.0f : azimuth);
    elevation = (elevation < -90.0f) ? -90.0f : (elevation > 90.0f ? 90.0f : elevation);

    // Find 3 nearest grid points
    struct Neighbor { float dist; int idx; };
    const int K = 3;
    Neighbor nearest[K];
    for (int i = 0; i < K; i++) nearest[i] = {1e9f, 0};

    for (int i = 0; i < (int)mHrirData.size(); i++) {
        float d = angularDistDeg(azimuth, elevation,
                                  mHrirData[i].azimuth, mHrirData[i].elevation);
        if (d < nearest[K-1].dist) {
            nearest[K-1] = {d, i};
            // Insertion sort (K=3, tiny)
            for (int j = K-1; j > 0 && nearest[j].dist < nearest[j-1].dist; j--)
                std::swap(nearest[j], nearest[j-1]);
        }
    }

    // Exact match
    if (nearest[0].dist < 0.001f) {
        memcpy(irL, mHrirData[nearest[0].idx].left.data(),  HRTF_IR_SIZE * sizeof(float));
        memcpy(irR, mHrirData[nearest[0].idx].right.data(), HRTF_IR_SIZE * sizeof(float));
        return;
    }

    // Inverse-distance-squared weighting
    float weights[K], totalWeight = 0.0f;
    for (int i = 0; i < K; i++) {
        weights[i]   = 1.0f / (nearest[i].dist * nearest[i].dist + 1e-6f);
        totalWeight += weights[i];
    }

    memset(irL, 0, HRTF_IR_SIZE * sizeof(float));
    memset(irR, 0, HRTF_IR_SIZE * sizeof(float));

    for (int i = 0; i < K; i++) {
        float w = weights[i] / totalWeight;
        const HrirEntry &e = mHrirData[nearest[i].idx];
        for (int k = 0; k < HRTF_IR_SIZE; k++) {
            irL[k] += w * e.left[k];
            irR[k] += w * e.right[k];
        }
    }
}

FilterInstance *HrtfFilter::createInstance()
{
    return new HrtfFilterInstance(this);
}

int HrtfFilter::getParamCount() { return 4; }

const char *HrtfFilter::getParamName(unsigned int aParamIndex)
{
    switch (aParamIndex) {
        case AZIMUTH:        return "Azimuth";
        case ELEVATION:      return "Elevation";
        case WET:            return "Wet";
        case CROSSOVER_FREQ: return "CrossoverFreq";
    }
    return "N/A";
}

unsigned int HrtfFilter::getParamType(unsigned int aParamIndex)
{
    return Filter::FLOAT_PARAM;
}

float HrtfFilter::getParamMax(unsigned int aParamIndex)
{
    switch (aParamIndex) {
        case AZIMUTH:        return 90.0f;
        case ELEVATION:      return 90.0f;
        case WET:            return 1.0f;
        case CROSSOVER_FREQ: return 2000.0f;
    }
    return 1.0f;
}

float HrtfFilter::getParamMin(unsigned int aParamIndex)
{
    switch (aParamIndex) {
        case AZIMUTH:        return -90.0f;
        case ELEVATION:      return -90.0f;
        case WET:            return 0.0f;
        case CROSSOVER_FREQ: return 20.0f;
    }
    return 0.0f;
}

// --------------------------------------------------------------------------
// HrtfFilterInstance
// --------------------------------------------------------------------------

HrtfFilterInstance::HrtfFilterInstance(HrtfFilter *aParent)
    : mParent(aParent), mInited(false), mSamplerate(44100.0f),
      mHistPos(0), mCrossfadePos(CROSSFADE_SAMPLES),
      mLastAzimuth(9999.0f), mLastElevation(9999.0f),
      mLastCrossoverFreq(0.0f)
{
    initParams(4);
    mParam[HrtfFilter::AZIMUTH]        = mParent->mAzimuth;
    mParam[HrtfFilter::ELEVATION]      = mParent->mElevation;
    mParam[HrtfFilter::WET]            = mParent->mWet;
    mParam[HrtfFilter::CROSSOVER_FREQ] = mParent->mCrossoverFreq;

    mHistory.fill(0.0f);

    // Identity HRIRs (dirac = passthrough)
    mCurrentIrL.fill(0.0f); mCurrentIrL[0] = 1.0f;
    mCurrentIrR.fill(0.0f); mCurrentIrR[0] = 1.0f;
    mTargetIrL = mCurrentIrL;
    mTargetIrR = mCurrentIrR;
    mWorkIrL   = mCurrentIrL;
    mWorkIrR   = mCurrentIrR;

    mLoState = {0, 0, 0, 0};
    mHiState = {0, 0, 0, 0};
    mLoCoeffs = {0, 0, 0, 0, 0};
    mHiCoeffs = {0, 0, 0, 0, 0};
}

HrtfFilterInstance::~HrtfFilterInstance() {}

// Biquad coefficients from Audio EQ Cookbook (R. Bristow-Johnson, public domain).
// Q = 1/sqrt(2) gives Butterworth (maximally flat) response.
void HrtfFilterInstance::updateCrossover(float freq, float samplerate)
{
    float w0    = 2.0f * (float)M_PI * freq / samplerate;
    float cosw0 = cosf(w0);
    float sinw0 = sinf(w0);
    float alpha = sinw0 * 0.7071f; // sin(w0) / (2 * Q) with Q=1/sqrt(2)

    // Low-pass
    {
        float b0 = (1.0f - cosw0) * 0.5f;
        float b1 = 1.0f - cosw0;
        float b2 = (1.0f - cosw0) * 0.5f;
        float a0 = 1.0f + alpha;
        float a1 = -2.0f * cosw0;
        float a2 = 1.0f - alpha;
        mLoCoeffs = {b0/a0, b1/a0, b2/a0, a1/a0, a2/a0};
    }
    // High-pass
    {
        float b0 = (1.0f + cosw0) * 0.5f;
        float b1 = -(1.0f + cosw0);
        float b2 = (1.0f + cosw0) * 0.5f;
        float a0 = 1.0f + alpha;
        float a1 = -2.0f * cosw0;
        float a2 = 1.0f - alpha;
        mHiCoeffs = {b0/a0, b1/a0, b2/a0, a1/a0, a2/a0};
    }
    mLastCrossoverFreq = freq;
}

float HrtfFilterInstance::processBiquad(float x, BiquadState &s, const BiquadCoeffs &c)
{
    float y = c.b0*x + c.b1*s.x1 + c.b2*s.x2 - c.a1*s.y1 - c.a2*s.y2;
    s.x2 = s.x1; s.x1 = x;
    s.y2 = s.y1; s.y1 = y;
    return y;
}

// Blend mCurrentIr* and mTargetIr* into mWorkIr* by [blend] (0=current, 1=target).
void HrtfFilterInstance::blendIr(float blend)
{
    float inv = 1.0f - blend;
    for (int k = 0; k < HRTF_IR_SIZE; k++) {
        mWorkIrL[k] = inv * mCurrentIrL[k] + blend * mTargetIrL[k];
        mWorkIrR[k] = inv * mCurrentIrR[k] + blend * mTargetIrR[k];
    }
}

void HrtfFilterInstance::filter(
    float *aBuffer,
    unsigned int aSamples,
    unsigned int aBufferSize,
    unsigned int aChannels,
    float aSamplerate,
    double aTime)
{
    updateParams(aTime);

    if (!mInited) {
        mSamplerate = aSamplerate;
        updateCrossover(mParam[HrtfFilter::CROSSOVER_FREQ], aSamplerate);
        mInited = true;
    }

    float azimuth      = mParam[HrtfFilter::AZIMUTH];
    float elevation    = mParam[HrtfFilter::ELEVATION];
    float wet          = mParam[HrtfFilter::WET];
    float crossoverFreq = mParam[HrtfFilter::CROSSOVER_FREQ];

    if (crossoverFreq != mLastCrossoverFreq)
        updateCrossover(crossoverFreq, aSamplerate);

    // Wait until HRTF data is loaded
    if (!mParent->mDataReady.load(std::memory_order_acquire))
        return;

    // Position changed → look up new HRIR and start crossfade
    float posThresh = 0.05f;
    if (fabsf(azimuth - mLastAzimuth) > posThresh ||
        fabsf(elevation - mLastElevation) > posThresh)
    {
        mParent->interpolate(azimuth, elevation, mTargetIrL.data(), mTargetIrR.data());
        // If already crossfading, let it restart from 0 with the new target
        mCrossfadePos = 0;
        mLastAzimuth  = azimuth;
        mLastElevation = elevation;

        // On first position set, also initialize current so we don't
        // crossfade from the identity dirac
        if (mCurrentIrL[0] == 1.0f && mCurrentIrL[1] == 0.0f) {
            mCurrentIrL = mTargetIrL;
            mCurrentIrR = mTargetIrR;
            mCrossfadePos = CROSSFADE_SAMPLES; // skip crossfade on first set
        }
    }

    // Compute blend for this block (use midpoint to reduce zipper noise)
    bool crossfading = (mCrossfadePos < CROSSFADE_SAMPLES);
    if (crossfading) {
        int mid = mCrossfadePos + (int)aSamples / 2;
        float blend = (float)mid / (float)CROSSFADE_SAMPLES;
        if (blend > 1.0f) blend = 1.0f;
        blendIr(blend);
        mCrossfadePos += (int)aSamples;
        if (mCrossfadePos >= CROSSFADE_SAMPLES) {
            mCurrentIrL = mTargetIrL;
            mCurrentIrR = mTargetIrR;
        }
    } else {
        mWorkIrL = mCurrentIrL;
        mWorkIrR = mCurrentIrR;
    }

    float *bufL = aBuffer;
    // For mono input, we still expand to stereo: the resample buffer is
    // allocated for MAX_CHANNELS, so aBuffer + aSamples is safe to write as R.
    // Downstream, the mixer bumps voice->mChannels to 2 (see expandsToStereo()).
    float *bufR = aBuffer + aSamples;

    for (unsigned int i = 0; i < aSamples; i++) {
        float mono = bufL[i];
        if (aChannels >= 2) mono = (mono + bufR[i]) * 0.5f;

        float loPass = processBiquad(mono, mLoState, mLoCoeffs);
        float hiPass = processBiquad(mono, mHiState, mHiCoeffs);

        // Push hi-pass sample into ring buffer
        mHistory[mHistPos] = hiPass;

        // Direct convolution: out[k] = sum_k ir[k] * history[now - k]
        float outL = 0.0f, outR = 0.0f;
        for (int k = 0; k < HRTF_IR_SIZE; k++) {
            int idx = mHistPos - k;
            if (idx < 0) idx += HRTF_IR_SIZE;
            outL += mWorkIrL[k] * mHistory[idx];
            outR += mWorkIrR[k] * mHistory[idx];
        }

        mHistPos++;
        if (mHistPos >= HRTF_IR_SIZE) mHistPos = 0;

        float dry = mono;
        bufL[i] = wet * (loPass + outL) + (1.0f - wet) * dry;
        bufR[i] = wet * (loPass + outR) + (1.0f - wet) * dry;
    }


}

} // namespace SoLoud
