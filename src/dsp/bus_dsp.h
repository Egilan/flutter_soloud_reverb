// Per-bus DSP state owned by BusData and reached from SoLoud::BusInstance via
// the opaque Bus::mDspHook pointer. Keeping this in its own header lets both the
// plugin layer (filters.h / player.cpp) and SoLoud core (soloud_bus.cpp)
// include it without SoLoud core depending on the wider plugin types.
#pragma once

#include "bus_meter.h"
#include "ducker.h"

namespace fsdsp {

struct BusDsp {
  unsigned int busId = 0;
  Ducker *ducker = nullptr;     // shared, owned by Player
  BusMeter meter;
  OnePole duckSmoother;         // per-bus duck gain ramp; snapped to 1.0
  std::atomic<bool> duckOptOut{false};

  BusDsp() { duckSmoother.snap(1.0f); }

  // Called from BusInstance::getAudio, AFTER mixBus_internal, on the planar bus
  // buffer. Applies ducking (key vs. non-key) then taps the meter POST-duck.
  void process(float *buffer, unsigned int samples, unsigned int stride,
               unsigned int channels, float sampleRate) {
    if (ducker) {
      unsigned int key = ducker->keyBusId.load(std::memory_order_relaxed);
      if (key != 0 && busId == key) {
        ducker->processKey(buffer, samples, stride, channels, sampleRate);
      } else if (!duckOptOut.load(std::memory_order_relaxed)) {
        ducker->applyDuck(buffer, samples, stride, channels, sampleRate,
                          duckSmoother);
      }
    }
    meter.process(buffer, samples, stride, channels, sampleRate);
  }
};

} // namespace fsdsp
