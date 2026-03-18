// pd_bridge.cpp
// FFI-exported patch control functions for libpd.
// Audio processing is NOT here — SoLoud calls libpd_process_float() directly
// inside its getAudio() callback (in flutter_soloud_reverb/src/synth/soloud_libpd.cpp).
// This file handles: patch lifecycle, parameter messaging, value callbacks.

#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include "z_libpd.h"

#define EXPORT extern "C" __attribute__((visibility("default")))

// ── Global state ─────────────────────────────────────────────────────────────

static std::once_flag g_init_flag;
static std::unordered_map<std::string, void*> g_patches;
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

// Called once to initialize libpd. Returns 0 on success, non-zero on failure.
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

// Opens a .pd patch file, tracked by patch_id.
// Returns 0 on success, 1 if already open with that ID, -1 on failure.
EXPORT int pd_open_patch(const char* patch_id, const char* dir, const char* filename) {
    std::lock_guard<std::mutex> lock(g_patch_mutex);
    std::string id(patch_id);
    if (g_patches.count(id)) return 1;
    void* handle = libpd_openfile(filename, dir);
    if (!handle) return -1;
    g_patches[id] = handle;
    return 0;
}

// Closes the patch identified by patch_id. No-op if not found.
EXPORT void pd_close_patch(const char* patch_id) {
    std::lock_guard<std::mutex> lock(g_patch_mutex);
    auto it = g_patches.find(std::string(patch_id));
    if (it != g_patches.end()) {
        libpd_closefile(it->second);
        g_patches.erase(it);
    }
}

// Closes all open patches.
EXPORT void pd_close_all_patches() {
    std::lock_guard<std::mutex> lock(g_patch_mutex);
    for (auto& kv : g_patches) {
        libpd_closefile(kv.second);
    }
    g_patches.clear();
}

// ── Messaging: Dart → PD ─────────────────────────────────────────────────────
// Note: PD receivers are global across ALL loaded patches.
// There is no per-patch targeting — the receiver name determines which
// [receive] objects in which patches respond.

// Send a float value to a named receiver. Returns 0 on success.
EXPORT int pd_send_float(const char* receiver, float value) {
    return libpd_float(receiver, value);
}

// Send a bang to a named receiver. Returns 0 on success.
EXPORT int pd_send_bang(const char* receiver) {
    return libpd_bang(receiver);
}

// ── Callbacks: PD → Dart ─────────────────────────────────────────────────────

// Register a Dart NativeCallable to receive float outputs from patches.
EXPORT void pd_set_float_hook(void (*callback)(const char* recv, float val)) {
    g_float_callback = callback;
}

// Register a Dart NativeCallable to receive bang outputs from patches.
EXPORT void pd_set_bang_hook(void (*callback)(const char* recv)) {
    g_bang_callback = callback;
}
