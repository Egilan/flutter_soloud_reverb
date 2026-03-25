# Dattorro Plate Reverb in Flutter Soloud

This document describes how to use the Dattorro plate reverb filter in Flutter Soloud, including as a bus filter and in combination with IR convolution reverb.

## Overview

The Dattorro reverb is a classic algorithmic plate reverb based on Jon Dattorro's 1997 paper "Effect Design Part 1: Reverberator and Other Filters". It provides a lush, dense reverb tail with real-time parameter control, making it ideal for scenarios where you need:

- Adjustable reverb without loading IR files
- Real-time parameter tweaking (decay, damping, pre-delay)
- Low memory footprint compared to convolution reverb
- Combination with IR reverb for hybrid processing

## Architecture

### C++ Layer

The implementation consists of two parts:

1. **Core DSP** (`src/filters/dattorro.h`, `src/filters/dattorro.cpp`):
   - `dattorro::DattorroReverb` class with the full Dattorro plate topology
   - 4-stage input diffusion (series allpass filters)
   - Figure-of-eight tank with two halves, each containing allpass + delay + lowpass + allpass + delay
   - LFO modulation (~1 Hz) on tank allpass filters for chorus-like density
   - Cross-feedback between tank halves
   - 7-tap output mixing from both tank halves
   - All buffer sizes scale with sample rate from the original 29761 Hz design

2. **SoLoud Filter Wrapper** (`src/filters/soloud_dattorro_filter.h`, `src/filters/soloud_dattorro_filter.cpp`):
   - `SoLoud::DattorroFilter` / `SoLoud::DattorroFilterInstance` classes
   - Integrates the Dattorro DSP into SoLoud's filter chain
   - Supports stereo and mono processing
   - Parameters are automatable via SoLoud's parameter system (fade, oscillate)

### Dart Layer

- **Filter type**: `FilterType.dattorroFilter` (in `lib/src/filters/filters.dart`)
- **Filter classes**: `DattorroGlobal`, `DattorroSingle` (in `lib/src/filters/dattorro_filter.dart`)
- **Parameter enum**: `DattorroEnum` with `preDelay`, `decay`, `damping`, `wet`, `dry`

## Parameters

| Parameter | Attribute ID | Range | Default | Description |
|-----------|-------------|-------|---------|-------------|
| `preDelay` | 0 | 0.0 - 1.0 | 0.0 | Pre-delay amount. 0.0 = no delay, 1.0 = 200ms. Adds a gap before the reverb onset. |
| `decay` | 1 | 0.0 - 0.99 | 0.7 | Reverb tail length. Higher values = longer reverb. Values near 0.99 produce very long, ambient tails. |
| `damping` | 2 | 0.0 - 0.99 | 0.5 | High-frequency absorption in the tank. Higher values darken the reverb tail faster. |
| `wet` | 3 | 0.0 - 1.0 | 0.8 | Volume of the reverb (wet) signal. |
| `dry` | 4 | 0.0 - 1.0 | 0.5 | Volume of the original (dry) signal. |

## API Reference

### Adding Dattorro Reverb to a Bus

```dart
// Create a bus
final bus = SoLoud.instance.createBus();

// Add the Dattorro filter
SoLoud.instance.addBusFilter(bus, FilterType.dattorroFilter);

// Set parameters
SoLoud.instance.setBusFilterParameter(bus, FilterType.dattorroFilter, 0, 0.1);  // preDelay
SoLoud.instance.setBusFilterParameter(bus, FilterType.dattorroFilter, 1, 0.85); // decay
SoLoud.instance.setBusFilterParameter(bus, FilterType.dattorroFilter, 2, 0.4);  // damping
SoLoud.instance.setBusFilterParameter(bus, FilterType.dattorroFilter, 3, 0.8);  // wet
SoLoud.instance.setBusFilterParameter(bus, FilterType.dattorroFilter, 4, 0.5);  // dry

// Play audio through the bus
await SoLoud.instance.playOnBus(bus, audioSource);
```

### Using as a Global Filter

```dart
// Activate globally
SoLoud.instance.filters.dattorroFilter.activate();

// Set parameters via the filter API
SoLoud.instance.filters.dattorroFilter.decay.value = 0.9;
SoLoud.instance.filters.dattorroFilter.damping.value = 0.6;
SoLoud.instance.filters.dattorroFilter.wet.value = 0.7;
SoLoud.instance.filters.dattorroFilter.dry.value = 0.4;
SoLoud.instance.filters.dattorroFilter.preDelay.value = 0.15;

// Deactivate when done
SoLoud.instance.filters.dattorroFilter.deactivate();
```

### Using as a Per-Sound Filter

```dart
final source = await SoLoud.instance.loadAsset('assets/audio/voice.mp3');

// Activate on this sound
source.filters.dattorroFilter.activate();

// Set params (requires a SoundHandle for per-sound control)
final handle = await SoLoud.instance.play(source);
source.filters.dattorroFilter.decay(soundHandle: handle).value = 0.8;
source.filters.dattorroFilter.damping(soundHandle: handle).value = 0.5;
```

### Removing from a Bus

```dart
SoLoud.instance.removeBusFilter(bus, FilterType.dattorroFilter);
```

## Hybrid Mode: Combining Dattorro with IR Convolution

You can stack both filters on a single bus. The convolution filter provides the spatial character of a real space, while the Dattorro adds adjustable tail length and density:

```dart
final bus = SoLoud.instance.createBus();

// Add IR convolution first
SoLoud.instance.addBusFilter(bus, FilterType.convolutionFilter);
final irSource = await SoLoud.instance.loadAsset('assets/audio/IR/hall.wav');
if (irSource.path != null) {
  SoLoud.instance.loadBusConvolutionIR(bus, irSource.path!);
}
await SoLoud.instance.disposeSource(irSource);

// Add Dattorro on top
SoLoud.instance.addBusFilter(bus, FilterType.dattorroFilter);
SoLoud.instance.setBusFilterParameter(bus, FilterType.dattorroFilter, 1, 0.6);  // decay
SoLoud.instance.setBusFilterParameter(bus, FilterType.dattorroFilter, 2, 0.3);  // damping
SoLoud.instance.setBusFilterParameter(bus, FilterType.dattorroFilter, 3, 0.5);  // wet (lower to blend)
SoLoud.instance.setBusFilterParameter(bus, FilterType.dattorroFilter, 4, 0.8);  // dry

// Play through the hybrid bus
await SoLoud.instance.playOnBus(bus, audioSource);
```

## Comparison: Dattorro vs IR Convolution vs ReverbSc

| Feature | Dattorro | IR Convolution | ReverbSc |
|---------|----------|----------------|----------|
| Algorithm | Dattorro plate topology | FFT convolution | Dattorro (simplified interface) |
| Parameters | preDelay, decay, damping, wet, dry | wet, dry + IR file | feedback, lpFreq, wet, dry |
| Pre-delay control | Yes (0-200ms) | Via IR file content | No |
| Decay control | Direct (0.0-0.99) | Determined by IR | Via feedback param |
| Memory | Low (fixed buffers) | Depends on IR length | Low (fixed buffers) |
| CPU | Low | Moderate (FFT) | Low |
| Realism | Plate character | Matches real spaces | Plate character |
| Real-time tweaking | All params | Wet/dry only | All params |

**Note**: `ReverbScFilter` and `DattorroFilter` both use the same underlying `dattorro::DattorroReverb` DSP engine. The difference is the parameter interface: ReverbSc exposes `feedback` (mapped to decay) and `lpFreq` (mapped to damping via frequency), while DattorroFilter exposes the native Dattorro parameters directly including pre-delay.

## Preset Suggestions

```dart
// Small room
SoLoud.instance.setBusFilterParameter(bus, FilterType.dattorroFilter, 0, 0.0);   // no pre-delay
SoLoud.instance.setBusFilterParameter(bus, FilterType.dattorroFilter, 1, 0.3);   // short decay
SoLoud.instance.setBusFilterParameter(bus, FilterType.dattorroFilter, 2, 0.7);   // high damping
SoLoud.instance.setBusFilterParameter(bus, FilterType.dattorroFilter, 3, 0.5);   // moderate wet
SoLoud.instance.setBusFilterParameter(bus, FilterType.dattorroFilter, 4, 0.8);   // mostly dry

// Large hall
SoLoud.instance.setBusFilterParameter(bus, FilterType.dattorroFilter, 0, 0.15);  // some pre-delay
SoLoud.instance.setBusFilterParameter(bus, FilterType.dattorroFilter, 1, 0.85);  // long decay
SoLoud.instance.setBusFilterParameter(bus, FilterType.dattorroFilter, 2, 0.3);   // low damping
SoLoud.instance.setBusFilterParameter(bus, FilterType.dattorroFilter, 3, 0.8);   // wet
SoLoud.instance.setBusFilterParameter(bus, FilterType.dattorroFilter, 4, 0.4);   // less dry

// Ambient / infinite
SoLoud.instance.setBusFilterParameter(bus, FilterType.dattorroFilter, 0, 0.2);   // pre-delay
SoLoud.instance.setBusFilterParameter(bus, FilterType.dattorroFilter, 1, 0.97);  // near-infinite decay
SoLoud.instance.setBusFilterParameter(bus, FilterType.dattorroFilter, 2, 0.6);   // moderate damping
SoLoud.instance.setBusFilterParameter(bus, FilterType.dattorroFilter, 3, 1.0);   // full wet
SoLoud.instance.setBusFilterParameter(bus, FilterType.dattorroFilter, 4, 0.0);   // no dry (100% reverb)
```

## Implementation Changes

### C++ Layer (`src/`)
- [`src/enums.h`](../src/enums.h): Added `DattorroFilter` to `FilterType` enum
- [`src/filters/soloud_dattorro_filter.h`](../src/filters/soloud_dattorro_filter.h): SoLoud::Filter wrapper header with 5 params
- [`src/filters/soloud_dattorro_filter.cpp`](../src/filters/soloud_dattorro_filter.cpp): Filter instance implementation
- [`src/filters/filters.cpp`](../src/filters/filters.cpp): Added `DattorroFilter` cases to `addFilter()` and `getFilterParamNames()`

### Dart Layer (`lib/src/`)
- [`lib/src/filters/dattorro_filter.dart`](../lib/src/filters/dattorro_filter.dart): `DattorroEnum`, `DattorroSingle`, `DattorroGlobal` classes
- [`lib/src/filters/filters.dart`](../lib/src/filters/filters.dart): Added `dattorroFilter` to `FilterType` enum, `FiltersSingle`, and `FiltersGlobal`

### Build Files
- [`android/CMakeLists.txt`](../android/CMakeLists.txt): Added `soloud_dattorro_filter.cpp` to sources
- [`windows/CMakeLists.txt`](../windows/CMakeLists.txt): Added `dattorro.cpp`, `soloud_reverbsc.cpp`, `soloud_dattorro_filter.cpp`
- [`linux/CMakeLists.txt`](../linux/CMakeLists.txt): Added `dattorro.cpp`, `soloud_reverbsc.cpp`, `soloud_dattorro_filter.cpp`
- [`ios/Classes/flutter_soloud.mm`](../ios/Classes/flutter_soloud.mm): Added `#include` for `soloud_dattorro_filter.cpp`
- [`macos/Classes/flutter_soloud.mm`](../macos/Classes/flutter_soloud.mm): Added `#include` for all three Dattorro source files

### Example App
- [`example/lib/main.dart`](../example/lib/main.dart): Updated to support per-bus reverb mode selection (None / IR / Dattorro / Hybrid) with full Dattorro parameter controls
