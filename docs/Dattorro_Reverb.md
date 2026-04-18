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
- **Parameter enum**: `DattorroEnum` with `preDelay`, `decay`, `damping`, `wet`, `dry`, `bandwidth`, `inputDiffusion`, `lfoRate`, `lfoDepth`

## Parameters

| Parameter | ID | Range | Default | Description |
|-----------|-----|-------|---------|-------------|
| `preDelay` | 0 | 0.0–1.0 | 0.0 | Silence before reverb onset. 0.0 = no gap, 1.0 = 200 ms. |
| `decay` | 1 | 0.0–0.99 | 0.7 | How long the tail sustains. ~0.5 = short room, ~0.85 = large hall, ~0.97+ = near-infinite ambient wash. |
| `damping` | 2 | 0.0–0.99 | 0.5 | High-frequency absorption inside the tank. 0 = bright metallic tail, 0.99 = dark muffled tail. |
| `wet` | 3 | 0.0–1.0 | 0.8 | Volume of the reverb (wet) signal. |
| `dry` | 4 | 0.0–1.0 | 0.5 | Volume of the original (dry) signal. |
| `bandwidth` | 5 | 0.0–1.0 | 1.0 | Low-pass filter on the input *before* it enters the reverb. 1.0 = fully transparent (all frequencies pass in). Lower values roll off the highs so only the warm body of the sound feeds the tank — prevents harsh transients from "pinging" the reverb. |
| `inputDiffusion` | 6 | 0.0–1.0 | 1.0 | How much the 4-stage allpass cascade smears the input before it enters the tank. 0 = no smearing (individual echoes audible), 1 = maximum blur (instant smooth cloud). For ambient/meditation always keep at 1.0. |
| `lfoRate` | 7 | 0.1–10.0 Hz | 1.0 | Speed of the internal pitch-modulation LFOs that act on the tank delay lines. 0.3–1 Hz gives the classic Lexicon chorus-swirl. Above ~3 Hz starts sounding like vibrato. |
| `lfoDepth` | 8 | 0.0–1.0 | 1.0 | Depth of the LFO modulation. 0 = completely static (can produce metallic resonances at long decays), 1 = full excursion (~16 samples at 29 kHz, scaled to sample rate). This is the "Lexicon secret sauce" — it is what makes the reverb sound organic rather than synthetic. |

### Parameter Deep-Dive

#### Pre-Delay (`preDelay`, 0–200 ms)

The C++ mapping is `ms = value * 200`. Pre-delay inserts a gap of silence between
the dry sound and the first moment the reverb tail begins. Its practical effect depends
entirely on the decay setting:

- **Short decay + no pre-delay** → small, tight room feel; sound and reverb fuse.
- **Long decay + pre-delay 50–120 ms** → the dry signal rings out cleanly, then a
  massive wash blooms in behind it. Ideal for vocals, affirmations, spoken-word.
- **Near-infinite decay + no pre-delay** → diffused ambient wash; the dry signal
  dissolves into the reverb cloud. Ideal for meditation textures and atmospheric pads.

Classic rule of thumb by time range:

| ms | Perceptual character |
|----|----------------------|
| 0 | Sound and reverb fused (plate/spring feel) |
| 5–20 ms | Small/medium room — reverb onset feels natural |
| 20–50 ms | Large hall — separation between source and space |
| 50–120 ms | Dramatic bloom; source heard clearly before wash |
| > 120 ms | Slapback effect before reverb; very theatrical |

#### Decay (`decay`, 0–0.99)

This is the feedback coefficient of the figure-of-eight tank. Each pass around the
loop, the signal is multiplied by `decay`. It is *not* a linear time value — it is
exponential feedback:

- `0.5` ≈ 1–2 second tail at typical settings
- `0.85` ≈ 5–8 seconds
- `0.95` ≈ 15–30+ seconds
- `0.99` ≈ effectively infinite (very slow fade)

The exact perceived decay time also depends on damping: higher damping absorbs energy
faster, so the *audible* tail is shorter than pure feedback math suggests.

#### Damping (`damping`, 0–0.99)

A single-pole lowpass filter applied to the signal inside the tank on every feedback
loop iteration. High frequencies are absorbed each time around:

- `0.0` → no absorption; the tail stays bright and metallic throughout its full length.
  Can sound harsh at long decays.
- `0.5` → moderate roll-off; tail starts bright then darkens over time.
- `0.7–0.8` → fast HF absorption; tail becomes warm and enveloping very quickly.
  Recommended for meditation and ambient use.
- `0.99` → extreme — almost all high frequencies are killed on the first reflection.

#### Bandwidth (`bandwidth`, 0–1)

A pre-diffusion input filter. Unlike damping (which acts inside the feedback loop and
only affects the tail), bandwidth filters the dry signal *before* it even enters the
reverb. The effect is:

- `1.0` → fully transparent; the reverb responds to the full frequency content of the
  source.
- `0.3–0.6` → rolls off the highs entering the tank. Sharp attack transients (bird
  chirps, consonants, synth plucks) are softened before diffusion, so the reverb
  responds only to the warm body of the sound. Prevents harsh "ping" artifacts at
  long decays.

Bandwidth and damping work together but differently: bandwidth shapes *what enters*
the tank; damping shapes *how it fades inside* the tank.

#### Input Diffusion (`inputDiffusion`, 0–1)

Controls the coefficient of the 4-stage allpass cascade that smears the input signal
before it hits the tank. This determines the *attack shape* of the reverb:

- `0.0` → no diffusion; you can hear individual flutter echoes from the allpass
  delays. Sounds like a multi-tap delay, not a reverb.
- `0.5` → partial smear; some echo structure is still audible at the onset.
- `1.0` → full diffusion; the onset is an immediate smooth cloud with no discrete
  echoes. Almost always the right choice for ambient/plate reverb.

#### LFO Rate (`lfoRate`, 0.1–10 Hz)

The Dattorro topology modulates two of the tank allpass read positions with sinusoidal
LFOs (90° apart for stereo width). This gentle pitch-shifting is what gives plate
reverbs their "alive" quality:

- `0.3–0.8 Hz` → very slow swirl; sounds like natural acoustic beating in a real space.
- `1.0 Hz` → Dattorro paper default; lush chorus effect.
- `2–4 Hz` → faster modulation; sounds more obviously chorused, almost tremolo-like.
- `> 5 Hz` → vibrato territory; useful for special effects but not naturalistic.

#### LFO Depth (`lfoDepth`, 0–1)

Scales the modulation excursion (the maximum number of samples the read position
moves). At `1.0` the excursion is ~16 samples at Dattorro's original 29 kHz design,
scaled to the actual sample rate:

- `0.0` → static tank; no pitch modulation. At long decays, this can produce audible
  metallic resonances as the feedback reinforces specific frequencies.
- `0.3–0.5` → subtle chorus; keeps the tail organic without being obvious.
- `1.0` → full excursion; maximum swirl and density. The Lexicon-style "magic" quality
  comes from this being turned up.

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
