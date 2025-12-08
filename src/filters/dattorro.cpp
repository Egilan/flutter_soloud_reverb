// src/filters/dattorro.cpp
#include "dattorro.h"

namespace dattorro {

DattorroReverb::DattorroReverb() 
    : mSampleRate(44100.0f)
    , mDecay(0.5f)
    , mDamping(0.5f)
    , mWet(0.5f)
    , mDry(0.5f)
    , mPreDelayLen(1)
    , mPreDelayIdx(0)
    , mDiff1Idx(0), mDiff2Idx(0), mDiff3Idx(0), mDiff4Idx(0)
    , mTank1Idx(0), mTank2Idx(0)
    , mTankAP1Idx(0), mTankAP2Idx(0)
    , mDampL(0.0f), mDampR(0.0f)
    , mTankOutL(0.0f), mTankOutR(0.0f)
    , mInitialized(false)
{
    std::memset(mPreDelay, 0, sizeof(mPreDelay));
    std::memset(mDiff1, 0, sizeof(mDiff1));
    std::memset(mDiff2, 0, sizeof(mDiff2));
    std::memset(mDiff3, 0, sizeof(mDiff3));
    std::memset(mDiff4, 0, sizeof(mDiff4));
    std::memset(mTank1, 0, sizeof(mTank1));
    std::memset(mTank2, 0, sizeof(mTank2));
    std::memset(mTankAP1, 0, sizeof(mTankAP1));
    std::memset(mTankAP2, 0, sizeof(mTankAP2));
}

void DattorroReverb::init(float sr) {
    if (sr < 8000.0f) sr = 44100.0f;
    mSampleRate = sr;
    
    // Scale factor from Dattorro's original 29761 Hz design
    float scale = sr / 29761.0f;
    
    // Pre-delay (start with minimal)
    mPreDelayLen = 1;
    mPreDelayIdx = 0;
    
    // Input diffuser lengths (scaled)
    mDiff1Len = (int)(142 * scale); if (mDiff1Len < 1) mDiff1Len = 1; if (mDiff1Len > 511) mDiff1Len = 511;
    mDiff2Len = (int)(107 * scale); if (mDiff2Len < 1) mDiff2Len = 1; if (mDiff2Len > 511) mDiff2Len = 511;
    mDiff3Len = (int)(379 * scale); if (mDiff3Len < 1) mDiff3Len = 1; if (mDiff3Len > 511) mDiff3Len = 511;
    mDiff4Len = (int)(277 * scale); if (mDiff4Len < 1) mDiff4Len = 1; if (mDiff4Len > 511) mDiff4Len = 511;
    
    // Tank delay lengths
    mTank1Len = (int)(4453 * scale); if (mTank1Len < 1) mTank1Len = 1; if (mTank1Len > TANK1_MAX-1) mTank1Len = TANK1_MAX-1;
    mTank2Len = (int)(3720 * scale); if (mTank2Len < 1) mTank2Len = 1; if (mTank2Len > TANK2_MAX-1) mTank2Len = TANK2_MAX-1;
    
    // Tank allpass lengths
    mTankAP1Len = (int)(672 * scale); if (mTankAP1Len < 1) mTankAP1Len = 1; if (mTankAP1Len > 1023) mTankAP1Len = 1023;
    mTankAP2Len = (int)(908 * scale); if (mTankAP2Len < 1) mTankAP2Len = 1; if (mTankAP2Len > 1023) mTankAP2Len = 1023;
    
    // Output tap positions (from tank delays)
    mTapL1 = (int)(266 * scale);  if (mTapL1 >= mTank1Len) mTapL1 = mTank1Len - 1;
    mTapL2 = (int)(2974 * scale); if (mTapL2 >= mTank1Len) mTapL2 = mTank1Len - 1;
    mTapL3 = (int)(1913 * scale); if (mTapL3 >= mTank2Len) mTapL3 = mTank2Len - 1;
    mTapL4 = (int)(1996 * scale); if (mTapL4 >= mTank2Len) mTapL4 = mTank2Len - 1;
    
    mTapR1 = (int)(353 * scale);  if (mTapR1 >= mTank2Len) mTapR1 = mTank2Len - 1;
    mTapR2 = (int)(3627 * scale); if (mTapR2 >= mTank2Len) mTapR2 = mTank2Len - 1;
    mTapR3 = (int)(1228 * scale); if (mTapR3 >= mTank1Len) mTapR3 = mTank1Len - 1;
    mTapR4 = (int)(2673 * scale); if (mTapR4 >= mTank1Len) mTapR4 = mTank1Len - 1;
    
    // Ensure taps are at least 1
    if (mTapL1 < 1) mTapL1 = 1; if (mTapL2 < 1) mTapL2 = 1;
    if (mTapL3 < 1) mTapL3 = 1; if (mTapL4 < 1) mTapL4 = 1;
    if (mTapR1 < 1) mTapR1 = 1; if (mTapR2 < 1) mTapR2 = 1;
    if (mTapR3 < 1) mTapR3 = 1; if (mTapR4 < 1) mTapR4 = 1;
    
    // Reset indices
    mPreDelayIdx = 0;
    mDiff1Idx = mDiff2Idx = mDiff3Idx = mDiff4Idx = 0;
    mTank1Idx = mTank2Idx = 0;
    mTankAP1Idx = mTankAP2Idx = 0;
    
    // Clear states
    mDampL = mDampR = 0.0f;
    mTankOutL = mTankOutR = 0.0f;
    
    // Clear all buffers
    std::memset(mPreDelay, 0, sizeof(mPreDelay));
    std::memset(mDiff1, 0, sizeof(mDiff1));
    std::memset(mDiff2, 0, sizeof(mDiff2));
    std::memset(mDiff3, 0, sizeof(mDiff3));
    std::memset(mDiff4, 0, sizeof(mDiff4));
    std::memset(mTank1, 0, sizeof(mTank1));
    std::memset(mTank2, 0, sizeof(mTank2));
    std::memset(mTankAP1, 0, sizeof(mTankAP1));
    std::memset(mTankAP2, 0, sizeof(mTankAP2));
    
    mInitialized = true;
}

void DattorroReverb::setDecay(float val) { 
    mDecay = val; 
    if (mDecay < 0.0f) mDecay = 0.0f;
    if (mDecay > 0.99f) mDecay = 0.99f;
}

void DattorroReverb::setDamping(float val) { 
    mDamping = val;
    if (mDamping < 0.0f) mDamping = 0.0f;
    if (mDamping > 0.99f) mDamping = 0.99f;
}

void DattorroReverb::setWet(float val) { 
    mWet = val; 
    if (mWet < 0.0f) mWet = 0.0f;
    if (mWet > 1.0f) mWet = 1.0f;
}

void DattorroReverb::setDry(float val) { 
    mDry = val; 
    if (mDry < 0.0f) mDry = 0.0f;
    if (mDry > 1.0f) mDry = 1.0f;
}

void DattorroReverb::setPreDelay(float val) { 
    mPreDelayLen = (int)(val * mSampleRate * 0.2f); // 0-200ms
    if (mPreDelayLen < 1) mPreDelayLen = 1;
    if (mPreDelayLen >= PREDELAY_MAX) mPreDelayLen = PREDELAY_MAX - 1;
}

void DattorroReverb::processSample(float inL, float inR, float& outL, float& outR) {
    if (!mInitialized) {
        outL = inL;
        outR = inR;
        return;
    }
    
    // Mix input to mono for reverb processing
    float input = (inL + inR) * 0.5f;
    
    // === PRE-DELAY ===
    mPreDelay[mPreDelayIdx] = input;
    int preReadIdx = mPreDelayIdx - mPreDelayLen;
    if (preReadIdx < 0) preReadIdx += PREDELAY_MAX;
    float preDelayed = mPreDelay[preReadIdx];
    mPreDelayIdx++;
    if (mPreDelayIdx >= PREDELAY_MAX) mPreDelayIdx = 0;
    
    // === INPUT DIFFUSION (4 allpass filters) ===
    // These smear the input to prevent distinct echoes
    float diffused = preDelayed;
    diffused = processAllpass(diffused, mDiff1, 512, mDiff1Idx, mDiff1Len, 0.75f);
    diffused = processAllpass(diffused, mDiff2, 512, mDiff2Idx, mDiff2Len, 0.75f);
    diffused = processAllpass(diffused, mDiff3, 512, mDiff3Idx, mDiff3Len, 0.625f);
    diffused = processAllpass(diffused, mDiff4, 512, mDiff4Idx, mDiff4Len, 0.625f);
    
    // === TANK PROCESSING (figure-of-eight topology) ===
    // Each tank feeds into the other for lush stereo
    
    // Left tank input: diffused signal + feedback from right tank
    float tankInL = diffused + mTankOutR * mDecay;
    
    // Allpass diffusion in tank
    float apOutL = processAllpass(tankInL, mTankAP1, 1024, mTankAP1Idx, mTankAP1Len, 0.5f);
    
    // Damping lowpass (1-pole) - higher damping = darker sound
    mDampL = mDampL * mDamping + apOutL * (1.0f - mDamping);
    
    // Write to tank delay and read from end
    mTank1[mTank1Idx] = mDampL;
    int tank1ReadIdx = mTank1Idx - mTank1Len;
    if (tank1ReadIdx < 0) tank1ReadIdx += TANK1_MAX;
    mTankOutL = mTank1[tank1ReadIdx];
    mTank1Idx++;
    if (mTank1Idx >= TANK1_MAX) mTank1Idx = 0;
    
    // Right tank input: diffused signal + feedback from left tank
    float tankInR = diffused + mTankOutL * mDecay;
    
    // Allpass diffusion in tank
    float apOutR = processAllpass(tankInR, mTankAP2, 1024, mTankAP2Idx, mTankAP2Len, 0.5f);
    
    // Damping lowpass
    mDampR = mDampR * mDamping + apOutR * (1.0f - mDamping);
    
    // Write to tank delay and read from end
    mTank2[mTank2Idx] = mDampR;
    int tank2ReadIdx = mTank2Idx - mTank2Len;
    if (tank2ReadIdx < 0) tank2ReadIdx += TANK2_MAX;
    mTankOutR = mTank2[tank2ReadIdx];
    mTank2Idx++;
    if (mTank2Idx >= TANK2_MAX) mTank2Idx = 0;
    
    // === OUTPUT TAPS ===
    // Multiple taps from both tanks create rich stereo output
    float wetL = readDelay(mTank1, TANK1_MAX, mTank1Idx, mTapL1)
               + readDelay(mTank1, TANK1_MAX, mTank1Idx, mTapL2)
               - readDelay(mTank2, TANK2_MAX, mTank2Idx, mTapL3)
               + readDelay(mTank2, TANK2_MAX, mTank2Idx, mTapL4);
    
    float wetR = readDelay(mTank2, TANK2_MAX, mTank2Idx, mTapR1)
               + readDelay(mTank2, TANK2_MAX, mTank2Idx, mTapR2)
               - readDelay(mTank1, TANK1_MAX, mTank1Idx, mTapR3)
               + readDelay(mTank1, TANK1_MAX, mTank1Idx, mTapR4);
    
    // Scale output (4 taps summed, so divide by 4)
    wetL *= 0.25f;
    wetR *= 0.25f;
    
    // === FINAL MIX ===
    outL = inL * mDry + wetL * mWet;
    outR = inR * mDry + wetR * mWet;
}

}