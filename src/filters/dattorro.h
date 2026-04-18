// src/filters/dattorro.h
#ifndef DATTORRO_H
#define DATTORRO_H

#include <cmath>
#include <cstring>

namespace dattorro {

class DattorroReverb {
public:
    DattorroReverb();

    void init(float sampleRate);
    void processSample(float inL, float inR, float& outL, float& outR);

    void setPreDelay(float val);
    void setDecay(float val);
    void setDamping(float val);
    void setWet(float val);
    void setDry(float val);
    void setBandwidth(float val);
    void setInputDiffusion(float val);
    void setLfoRate(float hz);
    void setLfoDepth(float val);

private:
    float mSampleRate;
    float mDecay;
    float mDamping;
    float mWet;
    float mDry;
    float mBandwidth;       // input lowpass before diffusion (0=dark, 1=bright)
    float mInputDiffusion;  // 0-1 scales allpass coefficients (0.75/0.625 at 1.0)

    // Pre-delay
    enum { PREDELAY_MAX = 8820 }; // 200ms at 44.1k
    float mPreDelay[PREDELAY_MAX];
    int mPreDelayLen;
    int mPreDelayIdx;

    // Input diffusers (4 allpass filters in series)
    float mDiff1[512], mDiff2[512], mDiff3[1024], mDiff4[512];
    int mDiff1Idx, mDiff2Idx, mDiff3Idx, mDiff4Idx;
    int mDiff1Len, mDiff2Len, mDiff3Len, mDiff4Len;

    // Tank delays (the main reverb body)
    // Dattorro paper has two delay lines per tank half
    enum {
        TANK_DELAY1_MAX = 8192,  // first delay in left half
        TANK_DELAY2_MAX = 8192,  // second delay in left half
        TANK_DELAY3_MAX = 8192,  // first delay in right half
        TANK_DELAY4_MAX = 8192   // second delay in right half
    };
    float mTankDelay1[TANK_DELAY1_MAX];
    float mTankDelay2[TANK_DELAY2_MAX];
    float mTankDelay3[TANK_DELAY3_MAX];
    float mTankDelay4[TANK_DELAY4_MAX];
    int mTankDelay1Idx, mTankDelay2Idx, mTankDelay3Idx, mTankDelay4Idx;
    int mTankDelay1Len, mTankDelay2Len, mTankDelay3Len, mTankDelay4Len;

    // Tank allpass diffusers (two per half: AP1/AP2 for left, AP3/AP4 for right)
    float mTankAP1[2048], mTankAP2[4096];
    float mTankAP3[2048], mTankAP4[8192];
    int mTankAP1Idx, mTankAP2Idx, mTankAP3Idx, mTankAP4Idx;
    int mTankAP1Len, mTankAP2Len, mTankAP3Len, mTankAP4Len;

    // Damping lowpass states
    float mDampL, mDampR;

    // Tank feedback storage
    float mTankOutL, mTankOutR;

    // Bandwidth lowpass state (1-pole, applied before input diffusion)
    float mBandwidthState;

    // LFO for tank delay modulation
    float mLfoPhase;
    float mLfoFreq;          // in Hz, controlled by setLfoRate
    float mLfoDepth;         // normalized 0-1, controlled by setLfoDepth
    float mLfoExcursion;     // derived: mLfoDepth * 16 * sampleRateScale, updated in processSample
    float mLfoExcursionScale; // 16 * sampleRateScale, set during init()

    // Output tap positions (from Dattorro paper, scaled at init)
    // Left output taps
    int mTapL_d1a, mTapL_d1b;   // from tank delay 1
    int mTapL_ap2;               // from tank allpass 2
    int mTapL_d2;                // from tank delay 2
    int mTapL_d3a;               // from tank delay 3
    int mTapL_ap4;               // from tank allpass 4
    int mTapL_d4;                // from tank delay 4
    // Right output taps
    int mTapR_d3a, mTapR_d3b;   // from tank delay 3
    int mTapR_ap4;               // from tank allpass 4
    int mTapR_d4;                // from tank delay 4
    int mTapR_d1a;               // from tank delay 1
    int mTapR_ap2;               // from tank allpass 2
    int mTapR_d2;                // from tank delay 2

    bool mInitialized;

    // Helper functions
    inline float readDelay(const float* buf, int maxLen, int writeIdx, int offset) const {
        int idx = writeIdx - offset;
        while (idx < 0) idx += maxLen;
        return buf[idx];
    }

    // Read from delay with fractional sample interpolation (for LFO modulation)
    inline float readDelayInterp(const float* buf, int maxLen, int writeIdx, float offset) const {
        int intOffset = (int)offset;
        float frac = offset - (float)intOffset;

        int idx0 = writeIdx - intOffset;
        while (idx0 < 0) idx0 += maxLen;
        int idx1 = idx0 - 1;
        if (idx1 < 0) idx1 += maxLen;

        return buf[idx0] * (1.0f - frac) + buf[idx1] * frac;
    }

    inline float processAllpass(float input, float* buf, int maxLen, int& writeIdx, int len, float coeff) {
        int readIdx = writeIdx - len;
        while (readIdx < 0) readIdx += maxLen;

        float delayed = buf[readIdx];
        float output = -coeff * input + delayed;
        buf[writeIdx] = input + coeff * output;

        writeIdx++;
        if (writeIdx >= maxLen) writeIdx = 0;

        return output;
    }

    // Modulated allpass: the read position is modulated by LFO
    inline float processAllpassMod(float input, float* buf, int maxLen, int& writeIdx, int len, float coeff, float modSamples) {
        float readOffset = (float)len + modSamples;
        if (readOffset < 1.0f) readOffset = 1.0f;
        if (readOffset > (float)(maxLen - 1)) readOffset = (float)(maxLen - 1);

        float delayed = readDelayInterp(buf, maxLen, writeIdx, readOffset);
        float output = -coeff * input + delayed;
        buf[writeIdx] = input + coeff * output;

        writeIdx++;
        if (writeIdx >= maxLen) writeIdx = 0;

        return output;
    }
};

}
#endif
