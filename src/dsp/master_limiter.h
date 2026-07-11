// Master brickwall limiter: lookahead delay line + sliding-window maximum +
// smoothed gain envelope. No waveshaping anywhere -- pure multiplicative gain,
// so a sine stays a sine. Attack is implicit in the lookahead: the gain reaches
// its target before the offending sample exits the delay line.
//
// Latency added to the output equals `lookaheadMs` (default 3 ms), constant
// whether bypassed or not (bypass only forces the gain stage to unity through
// the release smoother, click-free). No locks, no allocations in process().
//
// See docs/DSP_Metering_Ducking_Limiter.md section 5.
#pragma once

#include "dsp_common.h"
#include <cstdint>
#include <vector>

namespace fsdsp {

class MasterLimiter {
public:
  // Called from Player::init (NOT the audio thread). Sizes for the maximum
  // supported lookahead (5 ms) and channel count so runtime changes never
  // allocate.
  void init(float sampleRate, unsigned int maxChannels,
            unsigned int maxBlockSize) {
    sr_ = sampleRate;
    maxLa_ = (unsigned)(0.005f * sampleRate) + 1;
    maxChannels_ = maxChannels;
    (void)maxBlockSize;
    delay_.assign((size_t)maxLa_ * maxChannels, 0.0f);
    // Monotonic-deque ring for the sliding max. Window never exceeds maxLa_,
    // so capacity maxLa_ + 2 is always enough.
    dqCap_ = maxLa_ + 2;
    dqVal_.assign(dqCap_, 0.0f);
    dqIdx_.assign(dqCap_, 0);
    resetState_();
    gain_.snap(1.0f);
  }

  // ---- audio thread ----  planar buffer, in place.
  void process(float *buffer, unsigned int samples, unsigned int stride,
               unsigned int channels) {
    if (maxLa_ == 0) return; // not initialized

    const bool bypassed = bypass.load(std::memory_order_relaxed);
    unsigned la = std::min(maxLa_ - 1,
        (unsigned)(0.001f * lookaheadMs.load(std::memory_order_relaxed) * sr_));
    if (la < 1) la = 1;
    float ceilLin = dbToLin(ceilingDb.load(std::memory_order_relaxed));
    relSmoother_.setTau(releaseSec.load(std::memory_order_relaxed), sr_);
    // Attack tau ties the gain ramp to the lookahead time.
    atkSmoother_.setTau((la / sr_) * 0.5f, sr_);

    // A change in the effective lookahead is a hard snap: reset the delay line
    // and sliding-max window (a single click) rather than gliding the latency.
    if (la != lastLa_) {
      resetState_();
      lastLa_ = la;
    }

    const unsigned int W = la; // sliding-max window length (samples ahead)
    float minG = 1.0f;
    for (unsigned int i = 0; i < samples; ++i) {
      // 1. Per-sample cross-channel peak of the INCOMING sample.
      float inPeak = 0.0f;
      for (unsigned int ch = 0; ch < channels; ++ch) {
        float a = std::fabs(buffer[i + ch * stride]);
        if (a > inPeak) inPeak = a;
      }
      // 2. Sliding max over the lookahead window (monotonic deque, O(1) amort).
      float winMax = pushAndMax_(inPeak, W);

      // 3. Target gain so the delayed sample (oldest in window) meets ceiling.
      float target = (!bypassed && winMax > ceilLin) ? (ceilLin / winMax) : 1.0f;

      // 4. Smooth: fast toward lower gain (attack), slow toward unity (release).
      float g = (target < gain_.state)
          ? (gain_.state = target + atkSmoother_.coeff * (gain_.state - target))
          : (gain_.state = target + relSmoother_.coeff * (gain_.state - target));
      if (g < minG) minG = g;

      // 5. Delay each channel by `la` and apply gain. Pure multiply.
      for (unsigned int ch = 0; ch < channels; ++ch) {
        float *dl = delay_.data() + (size_t)ch * maxLa_;
        float in = buffer[i + ch * stride];
        // read the sample written `la` iterations ago, then overwrite.
        unsigned int rpos = (dpos_ + maxLa_ - la) % maxLa_;
        float out = dl[rpos];
        dl[dpos_] = in;
        buffer[i + ch * stride] = out * g;
      }
      dpos_ = (dpos_ + 1) % maxLa_;
    }
    grLin_.store(minG, std::memory_order_relaxed); // most-reduced gain this block
  }

  // ---- FFI ----
  std::atomic<bool> bypass{false};
  AtomicParam ceilingDb{-0.3f};
  AtomicParam lookaheadMs{3.0f};   // clamped to [1, 5] in process()
  AtomicParam releaseSec{0.120f};  // 0.080-0.200
  float gainReduction() const { return grLin_.load(std::memory_order_relaxed); }

private:
  // Sliding-window maximum over the last W pushed values, using a monotonic
  // deque held in a ring buffer (dqVal_/dqIdx_, capacity dqCap_). O(1) amort,
  // no allocations. `idx_` is a monotonically increasing sample counter.
  float pushAndMax_(float v, unsigned int W) {
    if (W < 1) W = 1;
    // Pop smaller-or-equal values off the back (they can never be the max
    // while v is in the window).
    while (dqCount_ > 0) {
      unsigned int back = (dqHead_ + dqCount_ - 1) % dqCap_;
      if (dqVal_[back] <= v) {
        --dqCount_;
      } else {
        break;
      }
    }
    unsigned int tail = (dqHead_ + dqCount_) % dqCap_;
    dqVal_[tail] = v;
    dqIdx_[tail] = idx_;
    ++dqCount_;
    // Evict the front if it has fallen out of the window [idx_-W+1 .. idx_]:
    // front index <= idx_ - W  => outside window.
    while (dqCount_ > 0 && dqIdx_[dqHead_] + W <= idx_) {
      dqHead_ = (dqHead_ + 1) % dqCap_;
      --dqCount_;
    }
    ++idx_;
    return dqVal_[dqHead_];
  }

  void resetState_() {
    std::fill(delay_.begin(), delay_.end(), 0.0f);
    dpos_ = 0;
    dqHead_ = 0;
    dqCount_ = 0;
    idx_ = 0;
    gain_.snap(1.0f);
  }

  OnePole gain_, atkSmoother_, relSmoother_; // gain_.state is the envelope
  std::vector<float> delay_;                 // allocated in init() only
  std::vector<float> dqVal_;                 // monotonic-deque values (ring)
  std::vector<uint64_t> dqIdx_;              // monotonic-deque sample indices
  unsigned int dqCap_ = 0, dqHead_ = 0, dqCount_ = 0;
  uint64_t idx_ = 0;
  unsigned int dpos_ = 0, maxLa_ = 0, maxChannels_ = 0, lastLa_ = 0;
  float sr_ = 44100.0f;
  std::atomic<float> grLin_{1.0f};
};

} // namespace fsdsp
