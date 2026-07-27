// Shared DSP primitives for the fork's metering / ducking / limiting units.
// Header-only. See docs/DSP_Metering_Ducking_Limiter.md for the design.
//
// No locks, no allocations in the audio path. Cross-thread parameters are
// std::atomic scalars, read once per block on the audio thread.
#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>

namespace fsdsp {

inline float dbToLin(float db) { return std::pow(10.0f, db * 0.05f); }
inline float linToDb(float lin) {
  return 20.0f * std::log10(std::max(lin, 1.0e-9f)); // floor at ~-180 dBFS
}

/// One-pole smoother. setTau() is only called from the audio thread
/// (reading an atomic param each block), so no atomics needed inside.
struct OnePole {
  float coeff = 0.0f; // computed from tau
  float state = 0.0f;

  void setTau(float seconds, float sampleRate) {
    coeff = (seconds <= 0.0f) ? 0.0f
                              : std::exp(-1.0f / (seconds * sampleRate));
  }
  float process(float target) {
    state = target + coeff * (state - target);
    return state;
  }
  void snap(float v) { state = v; }
};

/// Attack/release follower on a rectified signal.
struct EnvFollower {
  OnePole atk, rel;
  float env = 0.0f;
  void setTimes(float atkSec, float relSec, float sr) {
    atk.setTau(atkSec, sr);
    rel.setTau(relSec, sr);
  }
  float process(float rectified) {
    float c = (rectified > env) ? atk.coeff : rel.coeff;
    env = rectified + c * (env - rectified);
    return env;
  }
};

/// Parameter cell: Dart/FFI thread writes, audio thread reads once per block.
using AtomicParam = std::atomic<float>;

} // namespace fsdsp
