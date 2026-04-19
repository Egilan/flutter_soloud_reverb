/// header file used by ffiGen to generate
/// lib/flutter_soloud_bindings_ffi_TMP.dart

// copy here the definition you would like to generate.
// go into "flutter_soloud" dir from the root project dir
// and run:
//
// dart run ffigen --config ffigen.yaml
// to generate [flutter_soloud_FFIGEN.dart], or if it complains for some reason:
// export CPATH="$(clang -v 2>&1 | grep "Selected GCC installation" | rev | cut -d' ' -f1 | rev)/include";  dart run ffigen --config ffigen.yaml
//
// the generated code will be placed into flutter_soloud_FFIGEN.dart
// copy the generated definition into flutter_soloud_bindings_ffi.dart

#include <stdbool.h>

#include "enums.h"
#include "audiobuffer/metadata_ffi.h"

#define FFI_PLUGIN_EXPORT

//--------------------- copy here the new functions to generate

 FFI_PLUGIN_EXPORT enum PlayerErrors setBufferStream(
        unsigned int *hash,
        unsigned long maxBufferSize,
        int bufferingType,
        double bufferingTimeNeeds,
        unsigned int sampleRate,
        unsigned int channels,
        int format,
        dartOnBufferingCallback_t onBufferingCallback,
        dartOnMetadataCallback_t onMetadataCallback);

 FFI_PLUGIN_EXPORT enum PlayerErrors loadConvolutionIR(
        unsigned int soundHash,
        const char *irPath);

 FFI_PLUGIN_EXPORT enum PlayerErrors createBus(unsigned int *busHandle);
 FFI_PLUGIN_EXPORT void destroyBus(unsigned int busHandle);
 FFI_PLUGIN_EXPORT enum PlayerErrors playOnBus(
    unsigned int busHandle,
    unsigned int soundHash,
    float volume,
    float pan,
    bool paused,
    bool looping,
    double loopingStartAt,
    unsigned int *voiceHandle);
 FFI_PLUGIN_EXPORT void setBusVolume(unsigned int busHandle, float volume);
 FFI_PLUGIN_EXPORT enum PlayerErrors addBusFilter(unsigned int busHandle, int filterType);
 FFI_PLUGIN_EXPORT enum PlayerErrors removeBusFilter(unsigned int busHandle, int filterType);
 FFI_PLUGIN_EXPORT enum PlayerErrors loadBusConvolutionIR(unsigned int busHandle, const char *irPath);
 FFI_PLUGIN_EXPORT void annexSoundToBus(unsigned int busHandle, unsigned int voiceHandle);

/// Enable or disable visualization data gathering for this bus.
FFI_PLUGIN_EXPORT void busSetVisualizationEnable(unsigned int busId, bool enable);

/// Calculate and return 256 floats of FFT data for this bus.
FFI_PLUGIN_EXPORT float *busCalcFFT(unsigned int busId);

/// Get 256 samples of wave data currently playing through this bus.
FFI_PLUGIN_EXPORT float *busGetWave(unsigned int busId);

/// Get the approximate output volume for a specific channel of this bus.
FFI_PLUGIN_EXPORT float busGetApproximateVolume(unsigned int busId, unsigned int channel);

/// Get the number of voices currently playing through this bus.
FFI_PLUGIN_EXPORT unsigned int busGetActiveVoiceCount(unsigned int busId);
