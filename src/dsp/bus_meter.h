// Per-bus peak + RMS meter with peak-hold and exponential decay.
// Audio thread writes atomics; FFI thread reads them. No buffers, no locks.
#pragma once

#include "dsp_common.h"

namespace fsdsp {

class BusMeter {
public:
  // ---- audio thread ----
  // Planar buffer: buffer[i + ch * stride]. Called once per block per bus.
  void process(const float *buffer, unsigned int samples,
               unsigned int stride, unsigned int channels,
               float sampleRate) {
    if (bypass.load(std::memory_order_relaxed)) return;

    float blockPeak = 0.0f;
    double sumSq = 0.0;
    for (unsigned int ch = 0; ch < channels; ++ch) {
      const float *p = buffer + ch * stride;
      for (unsigned int i = 0; i < samples; ++i) {
        float a = std::fabs(p[i]);
        if (a > blockPeak) blockPeak = a;
        sumSq += (double)p[i] * p[i];
      }
    }
    float rms = (samples && channels)
        ? (float)std::sqrt(sumSq / (double)(samples * channels)) : 0.0f;

    // Peak hold: hold holdSeconds, then exponential decay (~decayDbPerSec).
    holdCounter_ += samples;
    float held = heldPeak_.load(std::memory_order_relaxed);
    if (blockPeak >= held) {
      held = blockPeak;
      holdCounter_ = 0;
    } else if (holdCounter_ >
               (unsigned)(holdSeconds.load(std::memory_order_relaxed) *
                          sampleRate)) {
      float decay = std::pow(
          10.0f,
          -(decayDbPerSec.load(std::memory_order_relaxed) / 20.0f) *
              (samples / sampleRate));
      held *= decay;
      if (held < blockPeak) held = blockPeak;
    }
    heldPeak_.store(held, std::memory_order_relaxed);
    lastRms_.store(rms, std::memory_order_relaxed);
    lastRawPeak_.store(blockPeak, std::memory_order_relaxed);
  }

  // ---- FFI thread ----
  float peak() const { return heldPeak_.load(std::memory_order_relaxed); }
  float rawPeak() const { return lastRawPeak_.load(std::memory_order_relaxed); }
  float rms() const { return lastRms_.load(std::memory_order_relaxed); }

  std::atomic<bool> bypass{false};
  AtomicParam holdSeconds{1.0f};
  AtomicParam decayDbPerSec{20.0f};

private:
  std::atomic<float> heldPeak_{0.0f}, lastRawPeak_{0.0f}, lastRms_{0.0f};
  unsigned int holdCounter_ = 0; // audio thread only
};

} // namespace fsdsp
