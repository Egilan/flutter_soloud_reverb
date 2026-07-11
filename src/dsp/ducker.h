// Sidechain ducker. One global Ducker owned by Player; per-bus participation
// state lives in BusDsp. The key bus computes an envelope; other buses read the
// previous block's envelope and apply smoothed, soft-knee gain reduction capped
// at maxDepthDb. No locks, no allocations. See the design doc for the latency
// rationale (envelope may be one block old — inaudible at 10 ms attack).
#pragma once

#include "dsp_common.h"

namespace fsdsp {

class Ducker {
public:
  // ---------- audio thread: called from the KEY bus's getAudio ----------
  void processKey(const float *buffer, unsigned int samples,
                  unsigned int stride, unsigned int channels,
                  float sampleRate) {
    follower_.setTimes(attackSec.load(std::memory_order_relaxed),
                       releaseSec.load(std::memory_order_relaxed), sampleRate);
    // Envelope on the per-sample max across channels (peak-style key).
    float env = follower_.env;
    for (unsigned int i = 0; i < samples; ++i) {
      float rect = 0.0f;
      for (unsigned int ch = 0; ch < channels; ++ch) {
        float a = std::fabs(buffer[i + ch * stride]);
        if (a > rect) rect = a;
      }
      env = follower_.process(rect);
    }
    keyEnv_.store(env, std::memory_order_relaxed);
  }

  // ---------- audio thread: called from each NON-key bus's getAudio ----------
  // Scales the buffer in place. `smoother` is per-bus state (lives in BusDsp)
  // so each bus ramps independently and un-bypass is click-free.
  void applyDuck(float *buffer, unsigned int samples, unsigned int stride,
                 unsigned int channels, float sampleRate, OnePole &smoother) {
    float targetGain = 1.0f;
    if (!bypass.load(std::memory_order_relaxed)) {
      float envDb = linToDb(keyEnv_.load(std::memory_order_relaxed));
      float thrDb = thresholdDb.load(std::memory_order_relaxed);
      float r = std::max(1.0f, ratio.load(std::memory_order_relaxed));
      float knee = kneeDb.load(std::memory_order_relaxed); // total width
      float over = envDb - thrDb;

      // Soft-knee downward gain computer (standard second-order knee).
      float grDb = 0.0f; // gain reduction applied to the OTHER buses, <= 0
      if (over > knee * 0.5f) {
        grDb = -over * (1.0f - 1.0f / r);
      } else if (over > -knee * 0.5f && knee > 0.0f) {
        float t = over + knee * 0.5f; // 0..knee
        grDb = -(1.0f - 1.0f / r) * (t * t) / (2.0f * knee);
      }
      float maxDepth = maxDepthDb.load(std::memory_order_relaxed);
      if (grDb < -maxDepth) grDb = -maxDepth; // beds never disappear
      targetGain = dbToLin(grDb);
    }
    smoother.setTau(gainSmoothSec.load(std::memory_order_relaxed), sampleRate);
    for (unsigned int i = 0; i < samples; ++i) {
      float g = smoother.process(targetGain); // per-sample ramp: no zipper
      for (unsigned int ch = 0; ch < channels; ++ch)
        buffer[i + ch * stride] *= g;
    }
  }

  // ---------- FFI-settable parameters ----------
  std::atomic<bool> bypass{false};
  AtomicParam thresholdDb{-20.0f};
  AtomicParam ratio{2.0f};
  AtomicParam kneeDb{6.0f};
  AtomicParam attackSec{0.010f};
  AtomicParam releaseSec{0.250f};
  AtomicParam maxDepthDb{6.0f};       // hard cap on attenuation (positive dB)
  AtomicParam gainSmoothSec{0.010f};
  std::atomic<uint32_t> keyBusId{0};  // 0 = no key -> ducker inert

  // Debug/metering: current key envelope (linear).
  float currentKeyEnv() const { return keyEnv_.load(std::memory_order_relaxed); }

private:
  EnvFollower follower_;              // audio thread only
  std::atomic<float> keyEnv_{0.0f};
};

} // namespace fsdsp
