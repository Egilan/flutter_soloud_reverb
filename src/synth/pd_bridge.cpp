// pd_bridge.cpp
// FFI-exported patch control functions for libpd.
// Audio processing is NOT here — SoLoud calls libpd_process_float() directly
// inside its getAudio() callback (in flutter_soloud_reverb/src/synth/soloud_libpd.cpp).
// This file handles: patch lifecycle, parameter messaging, value callbacks.

#include <cstring>
#include <mutex>
#include "z_libpd.h"

#define EXPORT extern "C" __attribute__((visibility("default")))

// ── Global state ─────────────────────────────────────────────────────────────

static std::once_flag g_init_flag;
static void* g_patch = nullptr;
static std::mutex g_patch_mutex;

// Dart-registered callbacks (set via pd_set_*_hook)
static void (*g_float_callback)(const char* recv, float val) = nullptr;
static void (*g_bang_callback)(const char* recv) = nullptr;

// ── Internal libpd hooks (called on the audio thread) ────────────────────────

static void on_float(const char* recv, float val) {
    if (g_float_callback) g_float_callback(recv, val);
}

static void on_bang(const char* recv) {
    if (g_bang_callback) g_bang_callback(recv);
}

// ── Init ─────────────────────────────────────────────────────────────────────

// Called once by SoLoud's LibPDAudioSource constructor.
// Returns 0 on success, non-zero on failure.
EXPORT int pd_bridge_init(int sample_rate, int channels) {
    int result = 0;
    std::call_once(g_init_flag, [&]() {
        result = libpd_init();
        if (result != 0) return;
        result = libpd_init_audio(channels, channels, sample_rate);
        libpd_set_floathook(on_float);
        libpd_set_banghook(on_bang);
    });
    return result;
}

// ── Patch lifecycle ───────────────────────────────────────────────────────────

// Opens a .pd patch file. path = full path to the file, including filename.
// Returns 1 on success, 0 on failure.
EXPORT int pd_open_patch(const char* dir, const char* filename) {
    std::lock_guard<std::mutex> lock(g_patch_mutex);
    if (g_patch) {
        libpd_closefile(g_patch);
        g_patch = nullptr;
    }
    g_patch = libpd_openfile(filename, dir);
    return g_patch != nullptr ? 1 : 0;
}

EXPORT void pd_close_patch() {
    std::lock_guard<std::mutex> lock(g_patch_mutex);
    if (g_patch) {
        libpd_closefile(g_patch);
        g_patch = nullptr;
    }
}

// ── Messaging: Dart → PD ─────────────────────────────────────────────────────

// Send a float value to a named receiver in the patch.
// Returns 0 on success.
EXPORT int pd_send_float(const char* receiver, float value) {
    return libpd_float(receiver, value);
}

// Send a bang to a named receiver in the patch.
// Returns 0 on success.
EXPORT int pd_send_bang(const char* receiver) {
    return libpd_bang(receiver);
}

// ── Callbacks: PD → Dart ─────────────────────────────────────────────────────

// Register a Dart NativeCallable to receive float outputs from the patch.
EXPORT void pd_set_float_hook(void (*callback)(const char* recv, float val)) {
    g_float_callback = callback;
}

// Register a Dart NativeCallable to receive bang outputs from the patch.
EXPORT void pd_set_bang_hook(void (*callback)(const char* recv)) {
    g_bang_callback = callback;
}
