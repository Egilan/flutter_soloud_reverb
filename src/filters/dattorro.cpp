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
    , mTankDelay1Idx(0), mTankDelay2Idx(0), mTankDelay3Idx(0), mTankDelay4Idx(0)
    , mTankAP1Idx(0), mTankAP2Idx(0), mTankAP3Idx(0), mTankAP4Idx(0)
    , mDampL(0.0f), mDampR(0.0f)
    , mTankOutL(0.0f), mTankOutR(0.0f)
    , mLfoPhase(0.0f)
    , mLfoFreq(1.0f)
    , mLfoExcursion(0.0f)
    , mInitialized(false)
{
    std::memset(mPreDelay, 0, sizeof(mPreDelay));
    std::memset(mDiff1, 0, sizeof(mDiff1));
    std::memset(mDiff2, 0, sizeof(mDiff2));
    std::memset(mDiff3, 0, sizeof(mDiff3));
    std::memset(mDiff4, 0, sizeof(mDiff4));
    std::memset(mTankDelay1, 0, sizeof(mTankDelay1));
    std::memset(mTankDelay2, 0, sizeof(mTankDelay2));
    std::memset(mTankDelay3, 0, sizeof(mTankDelay3));
    std::memset(mTankDelay4, 0, sizeof(mTankDelay4));
    std::memset(mTankAP1, 0, sizeof(mTankAP1));
    std::memset(mTankAP2, 0, sizeof(mTankAP2));
    std::memset(mTankAP3, 0, sizeof(mTankAP3));
    std::memset(mTankAP4, 0, sizeof(mTankAP4));
}

void DattorroReverb::init(float sr) {
    if (sr < 8000.0f) sr = 44100.0f;
    mSampleRate = sr;

    // Scale factor from Dattorro's original 29761 Hz design
    float scale = sr / 29761.0f;

    // Pre-delay (start with minimal)
    mPreDelayLen = 1;
    mPreDelayIdx = 0;

    // Input diffuser lengths (scaled) - from Dattorro paper
    mDiff1Len = (int)(142 * scale); if (mDiff1Len < 1) mDiff1Len = 1; if (mDiff1Len > 511) mDiff1Len = 511;
    mDiff2Len = (int)(107 * scale); if (mDiff2Len < 1) mDiff2Len = 1; if (mDiff2Len > 511) mDiff2Len = 511;
    mDiff3Len = (int)(379 * scale); if (mDiff3Len < 1) mDiff3Len = 1; if (mDiff3Len > 511) mDiff3Len = 511;
    mDiff4Len = (int)(277 * scale); if (mDiff4Len < 1) mDiff4Len = 1; if (mDiff4Len > 511) mDiff4Len = 511;

    // Tank topology per half (from Dattorro paper Figure 1):
    //   allpass1 -> delay1 -> lowpass -> allpass2 -> delay2
    //
    // Left half:  AP1(672) -> Delay1(4453) -> LP -> AP2(1800) -> Delay2(3720)
    // Right half: AP3(908) -> Delay3(4217) -> LP -> AP4(2656) -> Delay4(3163)

    // Tank allpass lengths
    mTankAP1Len = (int)(672 * scale);  if (mTankAP1Len < 1) mTankAP1Len = 1; if (mTankAP1Len > 1023) mTankAP1Len = 1023;
    mTankAP2Len = (int)(1800 * scale); if (mTankAP2Len < 1) mTankAP2Len = 1; if (mTankAP2Len > 2047) mTankAP2Len = 2047;
    mTankAP3Len = (int)(908 * scale);  if (mTankAP3Len < 1) mTankAP3Len = 1; if (mTankAP3Len > 1023) mTankAP3Len = 1023;
    mTankAP4Len = (int)(2656 * scale); if (mTankAP4Len < 1) mTankAP4Len = 1; if (mTankAP4Len > 2047) mTankAP4Len = 2047;

    // Tank delay lengths
    mTankDelay1Len = (int)(4453 * scale); if (mTankDelay1Len < 1) mTankDelay1Len = 1; if (mTankDelay1Len > TANK_DELAY1_MAX-1) mTankDelay1Len = TANK_DELAY1_MAX-1;
    mTankDelay2Len = (int)(3720 * scale); if (mTankDelay2Len < 1) mTankDelay2Len = 1; if (mTankDelay2Len > TANK_DELAY2_MAX-1) mTankDelay2Len = TANK_DELAY2_MAX-1;
    mTankDelay3Len = (int)(4217 * scale); if (mTankDelay3Len < 1) mTankDelay3Len = 1; if (mTankDelay3Len > TANK_DELAY3_MAX-1) mTankDelay3Len = TANK_DELAY3_MAX-1;
    mTankDelay4Len = (int)(3163 * scale); if (mTankDelay4Len < 1) mTankDelay4Len = 1; if (mTankDelay4Len > TANK_DELAY4_MAX-1) mTankDelay4Len = TANK_DELAY4_MAX-1;

    // LFO setup: ~1 Hz modulation with excursion of ~16 samples (at 29761 Hz)
    mLfoFreq = 1.0f;
    mLfoExcursion = 16.0f * scale;
    mLfoPhase = 0.0f;

    // Output tap positions (from Dattorro paper)
    // Left output = +d1[266] +d1[2974] -ap2[1913] +d2[1996] -d3[1990] -ap4[187] -d4[1066]
    mTapL_d1a = (int)(266 * scale);  if (mTapL_d1a < 1) mTapL_d1a = 1; if (mTapL_d1a >= mTankDelay1Len) mTapL_d1a = mTankDelay1Len - 1;
    mTapL_d1b = (int)(2974 * scale); if (mTapL_d1b < 1) mTapL_d1b = 1; if (mTapL_d1b >= mTankDelay1Len) mTapL_d1b = mTankDelay1Len - 1;
    mTapL_ap2 = (int)(1913 * scale); if (mTapL_ap2 < 1) mTapL_ap2 = 1; if (mTapL_ap2 >= mTankAP2Len) mTapL_ap2 = mTankAP2Len - 1;
    mTapL_d2  = (int)(1996 * scale); if (mTapL_d2 < 1) mTapL_d2 = 1; if (mTapL_d2 >= mTankDelay2Len) mTapL_d2 = mTankDelay2Len - 1;
    mTapL_d3a = (int)(1990 * scale); if (mTapL_d3a < 1) mTapL_d3a = 1; if (mTapL_d3a >= mTankDelay3Len) mTapL_d3a = mTankDelay3Len - 1;
    mTapL_ap4 = (int)(187 * scale);  if (mTapL_ap4 < 1) mTapL_ap4 = 1; if (mTapL_ap4 >= mTankAP4Len) mTapL_ap4 = mTankAP4Len - 1;
    mTapL_d4  = (int)(1066 * scale); if (mTapL_d4 < 1) mTapL_d4 = 1; if (mTapL_d4 >= mTankDelay4Len) mTapL_d4 = mTankDelay4Len - 1;

    // Right output = +d3[353] +d3[3627] -ap2[1228] +d4[2673] -d1[2111] -ap2[335] -d2[121]
    mTapR_d3a = (int)(353 * scale);  if (mTapR_d3a < 1) mTapR_d3a = 1; if (mTapR_d3a >= mTankDelay3Len) mTapR_d3a = mTankDelay3Len - 1;
    mTapR_d3b = (int)(3627 * scale); if (mTapR_d3b < 1) mTapR_d3b = 1; if (mTapR_d3b >= mTankDelay3Len) mTapR_d3b = mTankDelay3Len - 1;
    mTapR_ap4 = (int)(1228 * scale); if (mTapR_ap4 < 1) mTapR_ap4 = 1; if (mTapR_ap4 >= mTankAP4Len) mTapR_ap4 = mTankAP4Len - 1;
    mTapR_d4  = (int)(2673 * scale); if (mTapR_d4 < 1) mTapR_d4 = 1; if (mTapR_d4 >= mTankDelay4Len) mTapR_d4 = mTankDelay4Len - 1;
    mTapR_d1a = (int)(2111 * scale); if (mTapR_d1a < 1) mTapR_d1a = 1; if (mTapR_d1a >= mTankDelay1Len) mTapR_d1a = mTankDelay1Len - 1;
    mTapR_ap2 = (int)(335 * scale);  if (mTapR_ap2 < 1) mTapR_ap2 = 1; if (mTapR_ap2 >= mTankAP2Len) mTapR_ap2 = mTankAP2Len - 1;
    mTapR_d2  = (int)(121 * scale);  if (mTapR_d2 < 1) mTapR_d2 = 1; if (mTapR_d2 >= mTankDelay2Len) mTapR_d2 = mTankDelay2Len - 1;

    // Reset indices
    mPreDelayIdx = 0;
    mDiff1Idx = mDiff2Idx = mDiff3Idx = mDiff4Idx = 0;
    mTankDelay1Idx = mTankDelay2Idx = mTankDelay3Idx = mTankDelay4Idx = 0;
    mTankAP1Idx = mTankAP2Idx = mTankAP3Idx = mTankAP4Idx = 0;

    // Clear states
    mDampL = mDampR = 0.0f;
    mTankOutL = mTankOutR = 0.0f;

    // Clear all buffers
    std::memset(mPreDelay, 0, sizeof(mPreDelay));
    std::memset(mDiff1, 0, sizeof(mDiff1));
    std::memset(mDiff2, 0, sizeof(mDiff2));
    std::memset(mDiff3, 0, sizeof(mDiff3));
    std::memset(mDiff4, 0, sizeof(mDiff4));
    std::memset(mTankDelay1, 0, sizeof(mTankDelay1));
    std::memset(mTankDelay2, 0, sizeof(mTankDelay2));
    std::memset(mTankDelay3, 0, sizeof(mTankDelay3));
    std::memset(mTankDelay4, 0, sizeof(mTankDelay4));
    std::memset(mTankAP1, 0, sizeof(mTankAP1));
    std::memset(mTankAP2, 0, sizeof(mTankAP2));
    std::memset(mTankAP3, 0, sizeof(mTankAP3));
    std::memset(mTankAP4, 0, sizeof(mTankAP4));

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
    float diffused = preDelayed;
    diffused = processAllpass(diffused, mDiff1, 512, mDiff1Idx, mDiff1Len, 0.75f);
    diffused = processAllpass(diffused, mDiff2, 512, mDiff2Idx, mDiff2Len, 0.75f);
    diffused = processAllpass(diffused, mDiff3, 512, mDiff3Idx, mDiff3Len, 0.625f);
    diffused = processAllpass(diffused, mDiff4, 512, mDiff4Idx, mDiff4Len, 0.625f);

    // === LFO ===
    // Advance LFO phase (sinusoidal, ~1 Hz)
    mLfoPhase += mLfoFreq / mSampleRate;
    if (mLfoPhase >= 1.0f) mLfoPhase -= 1.0f;
    float lfoSin = sinf(mLfoPhase * 2.0f * 3.14159265f);
    float lfoCos = cosf(mLfoPhase * 2.0f * 3.14159265f);

    // === FIX #3: Latch both tank outputs before computing either new value ===
    float prevTankOutL = mTankOutL;
    float prevTankOutR = mTankOutR;

    // === TANK PROCESSING (figure-of-eight topology) ===
    // Each half: allpass1 -> delay1 -> lowpass -> allpass2(modulated) -> delay2
    // Cross-feedback: left uses prevTankOutR, right uses prevTankOutL

    // --- LEFT HALF ---
    float tankInL = diffused + prevTankOutR * mDecay;

    // First allpass (modulated by LFO)
    float apOutL1 = processAllpassMod(tankInL, mTankAP1, 1024, mTankAP1Idx, mTankAP1Len, -0.7f, lfoSin * mLfoExcursion);

    // First delay line
    mTankDelay1[mTankDelay1Idx] = apOutL1;
    int d1ReadIdx = mTankDelay1Idx - mTankDelay1Len;
    if (d1ReadIdx < 0) d1ReadIdx += TANK_DELAY1_MAX;
    float d1Out = mTankDelay1[d1ReadIdx];
    mTankDelay1Idx++;
    if (mTankDelay1Idx >= TANK_DELAY1_MAX) mTankDelay1Idx = 0;

    // Damping lowpass (1-pole)
    mDampL = mDampL * mDamping + d1Out * (1.0f - mDamping);

    // Second allpass
    float apOutL2 = processAllpass(mDampL * mDecay, mTankAP2, 2048, mTankAP2Idx, mTankAP2Len, 0.5f);

    // Second delay line
    mTankDelay2[mTankDelay2Idx] = apOutL2;
    int d2ReadIdx = mTankDelay2Idx - mTankDelay2Len;
    if (d2ReadIdx < 0) d2ReadIdx += TANK_DELAY2_MAX;
    mTankOutL = mTankDelay2[d2ReadIdx];
    mTankDelay2Idx++;
    if (mTankDelay2Idx >= TANK_DELAY2_MAX) mTankDelay2Idx = 0;

    // --- RIGHT HALF ---
    float tankInR = diffused + prevTankOutL * mDecay;

    // First allpass (modulated by LFO, phase-offset using cosine)
    float apOutR1 = processAllpassMod(tankInR, mTankAP3, 1024, mTankAP3Idx, mTankAP3Len, -0.7f, lfoCos * mLfoExcursion);

    // First delay line
    mTankDelay3[mTankDelay3Idx] = apOutR1;
    int d3ReadIdx = mTankDelay3Idx - mTankDelay3Len;
    if (d3ReadIdx < 0) d3ReadIdx += TANK_DELAY3_MAX;
    float d3Out = mTankDelay3[d3ReadIdx];
    mTankDelay3Idx++;
    if (mTankDelay3Idx >= TANK_DELAY3_MAX) mTankDelay3Idx = 0;

    // Damping lowpass (1-pole)
    mDampR = mDampR * mDamping + d3Out * (1.0f - mDamping);

    // Second allpass
    float apOutR2 = processAllpass(mDampR * mDecay, mTankAP4, 2048, mTankAP4Idx, mTankAP4Len, 0.5f);

    // Second delay line
    mTankDelay4[mTankDelay4Idx] = apOutR2;
    int d4ReadIdx = mTankDelay4Idx - mTankDelay4Len;
    if (d4ReadIdx < 0) d4ReadIdx += TANK_DELAY4_MAX;
    mTankOutR = mTankDelay4[d4ReadIdx];
    mTankDelay4Idx++;
    if (mTankDelay4Idx >= TANK_DELAY4_MAX) mTankDelay4Idx = 0;

    // === OUTPUT TAPS (from Dattorro paper) ===
    // Left output: taps from both halves
    float wetL = readDelay(mTankDelay1, TANK_DELAY1_MAX, mTankDelay1Idx, mTapL_d1a)
               + readDelay(mTankDelay1, TANK_DELAY1_MAX, mTankDelay1Idx, mTapL_d1b)
               - readDelay(mTankAP2, 2048, mTankAP2Idx, mTapL_ap2)
               + readDelay(mTankDelay2, TANK_DELAY2_MAX, mTankDelay2Idx, mTapL_d2)
               - readDelay(mTankDelay3, TANK_DELAY3_MAX, mTankDelay3Idx, mTapL_d3a)
               - readDelay(mTankAP4, 2048, mTankAP4Idx, mTapL_ap4)
               - readDelay(mTankDelay4, TANK_DELAY4_MAX, mTankDelay4Idx, mTapL_d4);

    // Right output: taps from both halves
    float wetR = readDelay(mTankDelay3, TANK_DELAY3_MAX, mTankDelay3Idx, mTapR_d3a)
               + readDelay(mTankDelay3, TANK_DELAY3_MAX, mTankDelay3Idx, mTapR_d3b)
               - readDelay(mTankAP4, 2048, mTankAP4Idx, mTapR_ap4)
               + readDelay(mTankDelay4, TANK_DELAY4_MAX, mTankDelay4Idx, mTapR_d4)
               - readDelay(mTankDelay1, TANK_DELAY1_MAX, mTankDelay1Idx, mTapR_d1a)
               - readDelay(mTankAP2, 2048, mTankAP2Idx, mTapR_ap2)
               - readDelay(mTankDelay2, TANK_DELAY2_MAX, mTankDelay2Idx, mTapR_d2);

    // Scale output (7 taps summed)
    wetL *= 0.14f;
    wetR *= 0.14f;

    // === FINAL MIX ===
    outL = inL * mDry + wetL * mWet;
    outR = inR * mDry + wetR * mWet;
}

}
