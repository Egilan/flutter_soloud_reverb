# Claude's role

NEVER ASSUME YOU KNOW WHAT IS BEHIND COMPLEX ISSUES. Always discuss with the user, convince me that things are the way they are.

I am also a very skilled, professional developer working in a professional environment. Don't blindly follow my ideas - offer new insights and options when we're stuck.
I am a very intelligent being who understands code and debugging - not everything needs to be explained.

Give honest feedback with objective analysis, not pandering.

When things are getting hard - let's add comprehensive, useful logging to understand what is happening, step by step.

# Project context

This is a custom fork of Flutter SoLoud, which adds Buses (supported natively by SoLoud) and custom reverbs (Dattorro, IR), soon HRTF.

## What's been built

The fork extends the upstream plugin across C++, FFI, and Dart layers. Bus-level filter control was wired end-to-end (`setBusFilterParameter` / `getBusFilterParameter`), and 3D spatial audio through buses was added via `play3dOnBus`, which calls `Bus::play3d()` internally so the `PROCESS_3D` flag is correctly set.
A minor AAudio stability fix adds a 200ms settle delay before stream reinit on device reroute events.

On the reverb side, the Dattorro plate reverb is exposed as a first-class `DattorroFilter` with a full Dart API covering all parameters — `preDelay`, `decay`, `damping`, `wet`, `dry`, plus the newer `bandwidth`, `inputDiffusion`, `lfoRate`, and `lfoDepth`.
The demo app uses a `DattorroParams` value object and a preset picker with grouped controls. IR convolution normalization was corrected from peak-norm to L2-norm so long-tail impulse responses don't blow up output gain.

LibPD is embedded in the plugin itself (`pd_bridge.cpp` compiled into `libflutter_soloud_plugin.so`), exposing a `PdBridge` Dart singleton for multi-patch lifecycle and float/bang messaging.
iOS ships pre-built vendored `.a` libraries (device + simulator) built via CMake, mirroring the opus/ogg pattern, with a `scripts/build_libpd_ios.sh` build script.

## Upstream merge — April 2026 (upstream v4.0.2)

Merged upstream `alnitak/flutter_soloud` main into this fork. Key upstream changes integrated:

- **Official Mixing Bus** (`lib/src/mixing_bus.dart`, `Bus`/`Buses` classes) — upstream added its own bus API via `createMixingBus()` returning a `Bus` object. This coexists with the fork's lower-level `createBus()` / `BusHandle` API. The fork's `play3dOnBus` is more correct for true bus-level 3D audio than upstream's `Bus.play3d()` which routes to the main engine.
- **Parametric EQ filter** — replaced the old `equalizerFilter` with `parametricEqFilter` (1–64 bands, FFT window 32–4096).
- **Improved pitch shift** — replaced `smbPitchShift` with signalsmith-stretch library.
- **Swift Package Manager support** — iOS/macOS restructured to `flutter_soloud/` subdirectory with xcframeworks.
- **`play()`, `play3d()`, `speechText()` are now synchronous** — remove `await` from all call sites.
- **AAudio 200ms settle delay** preserved inside upstream's new `ma_mutex_lock` wrapper in `soloud_miniaudio.cpp`.

### Merge conflict patterns to know for future merges

- `FilterBase(FilterType, soundHash)` → upstream added `busId` as 3rd arg: `FilterBase(FilterType, soundHash, null)`.
- `FilterParam(soundHandle, filterType, attrId, min, max)` → upstream added `busId` as 2nd arg: `FilterParam(soundHandle, null, filterType, attrId, min, max)`. Getter-style calls (no soundHandle) need `null, null` prepended.
- `bindings_player.dart` has two bus sections — the fork's `BusHandle`-typed section (authoritative) and upstream's `int`-typed section (must be removed/merged on future pulls).
- `mixing_bus.dart` uses `busHandle: BusHandle` (not `busId: int`) — upstream's version uses raw `int`.

#### Fork core patches to vendored SoLoud (DSP hooks — must survive merges)

- `src/soloud/include/soloud_bus.h`: `SoLoud::Bus` has `void *mDspHook = nullptr;` (opaque pointer to `fsdsp::BusDsp`, set in `Player::createBus`).
- `src/soloud/src/core/soloud_bus.cpp`: includes `dsp/bus_dsp.h`; `BusInstance::getAudio` calls `mDspHook->process(...)` right after `mixBus_internal` (per-bus ducking + metering on the planar bus buffer).
- `src/soloud/include/soloud.h`: `Soloud` has `MasterDspHook mMasterDspHook` + `void *mMasterDspCtx` (function pointer + context, near `mFlags`).
- `src/soloud/src/core/soloud.cpp`: `mix_internal` invokes `mMasterDspHook` on `mOutputScratch.mData` between the global filter loop and `clip_internal` (master brickwall limiter; PLANAR layout: sample i, channel ch at `[i + ch*aStride]`).
- `src/player.cpp` `Player::init`: clears `CLIP_ROUNDOFF` (`soloud.mFlags &= ~CLIP_ROUNDOFF`) so `clip_internal` is a hard clamp only — CLIP_ROUNDOFF is a cubic waveshaper and must stay off (the limiter guarantees the clamp never engages). Also calls `limiter.init(...)` and registers `Player::masterDspTrampoline` as the master hook.
- New `src/dsp/` directory (header-only: `dsp_common.h`, `bus_meter.h`, `ducker.h`, `master_limiter.h`, `bus_dsp.h`); `BusData` (src/filters/filters.h) owns a `fsdsp::BusDsp dsp;` member. See `docs/DSP_Metering_Ducking_Limiter.md`.
