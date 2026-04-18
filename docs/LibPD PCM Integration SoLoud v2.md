# Technical Brief: LibPD PCM Integration with SoLoud

## Executive Summary

LibPD's `processFloat()` can be called **directly** inside SoLoud's `getAudio()` callback. This is the standard libpd usage pattern - it's designed to be called from audio processing callbacks. No ring buffers, separate threads, or Dart isolate communication are needed for the audio path.

---

## 1. C++ Class/Method to Subclass

### Primary Integration Point

**Base Classes** (defined in [`src/soloud/include/soloud_audiosource.h`](src/soloud/include/soloud_audiosource.h:206)):

- **`SoLoud::AudioSource`** - Base class requiring:
  - `virtual AudioSourceInstance *createInstance() = 0;` 
  - Set `mBaseSamplerate`, `mChannels`, `mVolume`

- **`SoLoud::AudioSourceInstance`** - Per-playing-instance requiring:
  - `virtual unsigned int getAudio(float *aBuffer, unsigned int aSamplesToRead, unsigned int aBufferSize) = 0;`
  - `virtual bool hasEnded() = 0;`

### Implementation Pattern (Direct processFloat Call)

Following [`src/synth/basic_wave.cpp`](src/synth/basic_wave.cpp:38):

```cpp
// In a new file: src/synth/soloud_libpd.cpp
class LibPDInstance : public SoLoud::AudioSourceInstance {
public:
    unsigned int getAudio(float *aBuffer, unsigned int aSamplesToRead, unsigned int aBufferSize) override {
        // Call libpd processFloat directly - this IS the standard usage
        // processFloat expects input and output buffers
        float input[256];  // Stack-allocated, no heap allocation
        float* in = input; // or nullptr if not needed
        
        unsigned int samplesWritten = 0;
        while (samplesWritten < aSamplesToRead) {
            // processFloat processes one libpd tick
            processFloat(in, aBuffer + samplesWritten, 256);
            samplesWritten += 256;
            if (samplesWritten >= aSamplesToRead) break;
        }
        return samplesWritten;
    }
    
    bool hasEnded() override { 
        return false; // Continuous generation, never ends
    }
};

class LibPDAudioSource : public SoLoud::AudioSource {
public:
    LibPDAudioSource() {
        mBaseSamplerate = 44100; // Must match SoLoud sample rate
        mChannels = 2; // stereo
        mVolume = 1.0f;
    }
    
    SoLoud::AudioSourceInstance *createInstance() override { 
        return new LibPDInstance(); 
    }
};
```

---

## 2. Threading Constraints - REVISED ARCHITECTURE

### Question 1: Does processFloat() have thread affinity?

**Answer: NO** - processFloat() does NOT have thread affinity requirements. It can be called from any native thread. It's specifically designed for audio callback usage.

### Question 2: Does processFloat() satisfy real-time constraints?

**Answer: YES, with caveats:**
- Designed specifically for audio callback usage
- Does NOT inherently block or allocate
- **Requirements for real-time safety:**
  1. PD patch must be pre-loaded before playback starts
  2. No message sending (sendFloat, sendMessage) during audio processing
  3. Pre-allocate any temporary buffers on the stack (not heap)

### Thread Model

```
SoLoud Audio Thread (native)
    |
    +-- getAudio() called
    |       |
    |       +-- processFloat() called directly  <-- SAFE
    |               |
    +-- Filters applied (SoLoud internal)
    |
    +-- Output to audio device
```

**No ring buffer needed** - direct invocation in getAudio() is the correct approach.

---

## 3. Dart API Addition (Method Signatures Only)

### In `lib/src/bindings/bindings_player_ffi.dart`:

```dart
@override
({PlayerErrors error, SoundHash soundHash}) loadLibPDSource({
  required int sampleRate,
  required int channels,
});
```

### In `lib/src/soloud.dart`:

```dart
/// Load a LibPD audio source for real-time DSP processing.
/// 
/// LibPD's processFloat() will be called directly inside SoLoud's 
/// audio callback on the native audio thread.
/// 
/// [sampleRate] - Must match SoLoud's initialized sample rate
/// [channels] - Number of channels (1=mono, 2=stereo)
Future<AudioSource> loadLibPDSource({
  required int sampleRate,
  required int channels,
});
```

---

## 4. Bus/Filter Chain Integration

This architecture fully supports:
- **Per-source filters**: Via `AudioSource.setFilter()` - applied automatically by SoLoud
- **Bus routing**: Can play through any `Bus` instance  
- **Volume/pan**: Standard SoLoud controls via `play()` parameters
- **Multiple instances**: Each `play()` call creates a new `LibPDInstance`

---

## 5. Risks and Blockers

| Risk | Severity | Notes |
|------|----------|-------|
| **Sample rate mismatch** | High | SoLoud and libpd MUST use same sample rate |
| **Blocking in getAudio()** | High | Must not send messages to PD during playback |
| **Buffer size alignment** | Medium | libpd tick size vs SoLoud buffer size |
| **PD not initialized** | High | Must initialize libpd before first play |

---

## 6. Implementation Checklist

1. [x] Create `src/synth/soloud_libpd.cpp` - LibPDAudioSource + LibPDInstance
2. [x] Add libpd header/library to CMakeLists.txt / build files
3. [x] Add `loadLibPDSource()` in `src/bindings.cpp`
4. [x] Add FFI binding in `lib/src/bindings/bindings_player_ffi.dart`
5. [x] Add Dart API in `lib/src/soloud.dart`
6. [x] Initialize libpd before SoLoud init (or handle ordering) - **Implemented via PdBridge**
7. [ ] Test with basic PD patch generating sine wave - *No example provided yet*

### Additional Implementation (Not in Original Plan)

8. [x] Create `src/synth/pd_bridge.cpp` - Patch lifecycle and messaging FFI
9. [x] Add PdBridge Dart class in `lib/src/pd_bridge.dart`
10. [x] Add PdBridge FFI bindings in `lib/src/bindings/bindings_pd_ffi.dart`

These additional components provide:
- Patch open/close functionality (`openPatch`, `closePatch`, `closeAllPatches`)
- Float and bang message sending (`sendFloat`, `sendBang`)
- Initialization separate from SoLoud (`PdBridge.instance.init()`)

---

## References

### Implementation Files

- Audio source: [`src/synth/soloud_libpd.cpp`](src/synth/soloud_libpd.cpp) - LibPDAudioSource + LibPDInstance
- Bridge FFI: [`src/synth/pd_bridge.cpp`](src/synth/pd_bridge.cpp) - Patch lifecycle and messaging
- Player binding: [`src/player.cpp:496`](src/player.cpp:496) - loadLibPDSource()
- FFI exports: [`src/bindings.cpp:626`](src/bindings.cpp:626) - loadLibPDSource FFI

### Dart Files

- Public API: [`lib/src/soloud.dart:1161`](lib/src/soloud.dart:1161) - loadLibPDSource()
- PdBridge: [`lib/src/pd_bridge.dart`](lib/src/pd_bridge.dart) - Patch and message management
- PdBridge FFI: [`lib/src/bindings/bindings_pd_ffi.dart`](lib/src/bindings/bindings_pd_ffi.dart)
- Player FFI: [`lib/src/bindings/bindings_player_ffi.dart:647`](lib/src/bindings/bindings_player_ffi.dart:647)

### Base Classes

- SoLoud AudioSource: [`src/soloud/include/soloud_audiosource.h`](src/soloud/include/soloud_audiosource.h)
- Example callback: [`src/synth/basic_wave.cpp`](src/synth/basic_wave.cpp) - shows getAudio pattern

---

---

## External Documentation

For user-facing documentation and usage examples, see: [`docs/LibPD_Patch_Playback.md`](docs/LibPD_Patch_Playback.md)

---

# Comments

A few minor observations worth noting before implementation:

**SoLoud v2 — buffer size hardcoding:** The pseudocode has `processFloat(in, aBuffer + samplesWritten, 256)` with a hardcoded tick size of 256. LibPD's tick size is actually 64 samples by default, and the relationship between libpd ticks and SoLoud's `aSamplesToRead` needs to be worked out explicitly during implementation. Not a doc issue, just something to not copy-paste blindly.

**SoLoud v2 — "no message sending during audio processing":** This is flagged as a risk but not fully explained. The reason is that `PdBase.sendFloat()` etc. are not real-time safe — they can lock. Worth keeping in mind that parameter changes from Dart will need to go through a lock-free mechanism (atomic floats) rather than direct PdBase calls mid-playback.