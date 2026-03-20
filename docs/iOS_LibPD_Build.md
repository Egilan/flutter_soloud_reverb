# iOS LibPD Build: Why Pre-built Static Libraries?

This document explains why libpd is compiled ahead of time into `.a` files for iOS, rather than compiled from source during the Flutter build — and why the simpler approaches don't work.

## Background: How Flutter Plugins Compile Native Code on Each Platform

A Flutter plugin that uses C/C++ code needs to compile that code separately for each platform (Android, iOS, macOS, etc.). Each platform uses a completely different build system:

- **Android** uses **CMake** — a cross-platform build tool that's very flexible about where source files live. You can point it at files anywhere on disk using absolute or relative paths, no restrictions.
- **iOS** uses **CocoaPods** — Apple's dependency manager. CocoaPods reads a configuration file called a "podspec" that lives inside the plugin's `ios/` directory. This podspec tells CocoaPods which source files to compile, which headers to include, and which libraries to link.

This difference matters because of a subtle but critical limitation in CocoaPods.

## The Problem: CocoaPods Can't See Files Outside `ios/`

When Flutter sets up a plugin for iOS, it tells CocoaPods: "the source root for this plugin is the `ios/` directory." CocoaPods then resolves all file paths relative to that directory.

Our plugin's C/C++ source code lives in `src/` and our libpd submodule lives in `third_party/libpd/` — both are **outside** the `ios/` directory:

```
flutter_soloud_reverb/
├── ios/                          ← CocoaPods thinks THIS is the entire world
│   ├── flutter_soloud.podspec
│   ├── Classes/
│   │   └── flutter_soloud.mm     ← The one file CocoaPods actually compiles
│   └── libs/                     ← Pre-built libraries live here
├── src/                          ← Plugin's own C++ code (outside ios/)
│   └── synth/
│       ├── soloud_libpd.cpp
│       └── pd_bridge.cpp
└── third_party/
    └── libpd/                    ← ~100 C source files (outside ios/)
        ├── pure-data/src/
        └── libpd_wrapper/
```

When the podspec listed files like `../third_party/libpd/pure-data/src/**/*.c`, CocoaPods **silently ignored** them. No error, no warning — it just compiled nothing. This is a known CocoaPods limitation: it refuses to include source files that are outside the pod's root directory.

The result: linker errors like `Undefined symbol: _libpd_init` — the code that calls libpd functions was compiled, but libpd itself was never compiled, so the linker couldn't find the actual implementations.

## Why Android Doesn't Have This Problem

Android uses CMake, which has no such restriction. The `android/CMakeLists.txt` simply says:

```cmake
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../third_party/libpd" ...)
```

CMake follows the path, finds all the source files, compiles them, and links everything together. It doesn't care that the files are in a parent directory.

## The Forwarder Pattern (Partial Solution)

Flutter's own documentation acknowledges the CocoaPods path limitation. The recommended workaround is the "forwarder" pattern: create a single `.mm` file inside `ios/Classes/` that `#include`s the actual source files using relative paths:

```objc
// ios/Classes/flutter_soloud.mm
#include "../../src/flutter_soloud.cpp"    // works!
#include "../../src/filters/dattorro.cpp"  // works!
```

This works because `#include` is a C preprocessor directive — it's resolved by the **compiler**, not by CocoaPods. The compiler follows relative paths just fine. CocoaPods only sees the single `.mm` file, compiles it, and the `#include` directives pull in all the actual code.

This is how all of our plugin's own C++ code (`flutter_soloud.cpp`, filter code, etc.) gets compiled on iOS. We added `soloud_libpd.cpp` and `pd_bridge.cpp` to this forwarder as well.

### Why We Can't Use This Pattern for libpd Itself

We tried `#include`-ing all ~100 libpd C files into a single forwarder file. It failed with hundreds of "redefinition" errors. The reason: many of these C files define `static` variables and internal structs with the same names. When compiled as separate files (separate "translation units"), each file gets its own copy and there's no conflict. But when you `#include` them all into one file, they all share the same scope and the names collide.

In short: the forwarder pattern works for a handful of related `.cpp` files, but not for an entire library with 100+ independent C files.

## Approaches We Considered

### 1. Fix the podspec paths (doesn't work)

We tried changing `s.source = { :path => '..' }` to make the pod root the parent directory. CocoaPods accepted this, but Flutter hardcodes `PODS_TARGET_SRCROOT` to the `ios/` directory regardless. The source files still didn't appear in the Xcode project (verified by checking the generated `project.pbxproj`).

### 2. Use libpd as a CocoaPods dependency (impractical)

libpd ships its own podspec at `third_party/libpd/libpd.podspec`. We could theoretically add `s.dependency 'libpd'` to our podspec. But CocoaPods dependencies only work with published pods (from the CocoaPods trunk) or with pods specified in the app's `Podfile`. The `s.dependency` directive doesn't support local paths (`:path =>`). This would mean every app using our plugin would need to manually add a libpd entry to their Podfile — breaking the self-contained nature of the plugin.

### 3. Pre-build libpd as a static library (what we chose)

Compile libpd once, ahead of time, into `.a` files (static libraries). Commit those files to the repository. The podspec then links against them as "vendored libraries" — pre-compiled binaries that CocoaPods knows how to include without needing source files.

This is the **exact same pattern** already used in this project for opus, ogg, vorbis, and FLAC. Those libraries' `.a` files are in `ios/libs/` and have been working reliably.

## How the Build Works Now

### Pre-build step (run once, or when updating libpd)

Run `scripts/build_libpd_ios.sh`. This script:

1. Uses CMake to compile `third_party/libpd/` for iOS device (arm64 architecture)
2. Uses CMake again for the iOS simulator (arm64 + x86_64 architectures)
3. Copies the resulting `.a` files to `ios/libs/`
4. Copies the two public headers (`z_libpd.h`, `m_pd.h`) to `ios/include/libpd/`

The outputs are committed to the repo so that consumers of the plugin don't need CMake installed.

### At Flutter build time

1. CocoaPods reads the podspec, which lists only `Classes/**/*` as source files
2. It compiles `flutter_soloud.mm`, which `#include`s our C++ code (including `soloud_libpd.cpp` and `pd_bridge.cpp`)
3. Those bridge files call libpd functions like `libpd_init()` and `libpd_process_float()`
4. The linker finds those function implementations in the pre-built `libpd_iOS-device.a` (or `libpd_iOS-simulator.a` for simulator builds)
5. Everything links successfully

### Summary of files

| File | Purpose |
|------|---------|
| `ios/libs/libpd_iOS-device.a` | Pre-built libpd for physical iPhones/iPads |
| `ios/libs/libpd_iOS-simulator.a` | Pre-built libpd for the iOS simulator |
| `ios/include/libpd/z_libpd.h` | libpd's public API header |
| `ios/include/libpd/m_pd.h` | Pure Data's core header (required by z_libpd.h) |
| `ios/Classes/flutter_soloud.mm` | Forwarder that compiles all plugin C++ code |
| `ios/flutter_soloud.podspec` | Tells CocoaPods how to build and link everything |
| `scripts/build_libpd_ios.sh` | Reproduces the `.a` files from source |

## When You Need to Re-run the Build Script

- If you update the libpd submodule to a newer version
- If you change libpd build flags (e.g., enabling multi-instance support)
- If Apple drops support for an architecture and you need to rebuild

Run: `./scripts/build_libpd_ios.sh`

Prerequisites: CMake (`brew install cmake`) and Xcode command line tools.
