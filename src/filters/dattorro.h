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

private:
    float mSampleRate;
    float mDecay;
    float mDamping;
    float mWet;
    float mDry;
    
    // Pre-delay
    enum { PREDELAY_MAX = 8820 }; // 200ms at 44.1k
    float mPreDelay[PREDELAY_MAX];
    int mPreDelayLen;
    int mPreDelayIdx;
    
    // Input diffusers (4 allpass filters in series)
    enum { 
        DIFF1_LEN = 142,
        DIFF2_LEN = 107,
        DIFF3_LEN = 379,
        DIFF4_LEN = 277
    };
    float mDiff1[512], mDiff2[512], mDiff3[512], mDiff4[512];
    int mDiff1Idx, mDiff2Idx, mDiff3Idx, mDiff4Idx;
    int mDiff1Len, mDiff2Len, mDiff3Len, mDiff4Len;
    
    // Tank delays (the main reverb body)
    enum {
        TANK1_MAX = 8192,
        TANK2_MAX = 8192
    };
    float mTank1[TANK1_MAX];
    float mTank2[TANK2_MAX];
    int mTank1Idx, mTank2Idx;
    int mTank1Len, mTank2Len;
    
    // Tank allpass diffusers
    enum {
        TANKAP1_LEN = 672,
        TANKAP2_LEN = 908
    };
    float mTankAP1[1024], mTankAP2[1024];
    int mTankAP1Idx, mTankAP2Idx;
    int mTankAP1Len, mTankAP2Len;
    
    // Damping lowpass states
    float mDampL, mDampR;
    
    // Tank feedback storage
    float mTankOutL, mTankOutR;
    
    // Output tap positions (scaled at init)
    int mTapL1, mTapL2, mTapL3, mTapL4;
    int mTapR1, mTapR2, mTapR3, mTapR4;
    
    bool mInitialized;
    
    // Helper functions
    inline float readDelay(const float* buf, int maxLen, int writeIdx, int offset) const {
        int idx = writeIdx - offset;
        while (idx < 0) idx += maxLen;
        return buf[idx];
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
};

}
#endif