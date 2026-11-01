// pd_bridge.cpp
// FFI-exported patch control functions for libpd.
// Audio processing goes through the shared PdRenderer on SoLoud's audio thread
// (in flutter_soloud_reverb/src/synth/soloud_libpd.cpp).
// This file handles: patch lifecycle, parameter messaging, value callbacks.

#include <string>
#include <unordered_map>
#include "pd_runtime.h"
#include "z_libpd.h"

#if defined(_WIN32)
#define EXPORT extern "C" __declspec(dllexport)
#else
#define EXPORT extern "C" __attribute__((visibility("default")))
#endif

// ── Global state ─────────────────────────────────────────────────────────────

PdRuntime &pdRuntime()
{
    static PdRuntime *runtime = new PdRuntime;
    return *runtime;
}
static std::unordered_map<std::string, void*> g_patches;

// Dart-registered callbacks (set via pd_set_*_hook)
static void (*g_float_callback)(const char* recv, float val) = nullptr;
static void (*g_bang_callback)(const char* recv) = nullptr;

// ── Internal libpd hooks (called during processing or messaging) ────────────────────────

static void on_float(const char* recv, float val) {
    if (g_float_callback) g_float_callback(recv, val);
}

static void on_bang(const char* recv) {
    if (g_bang_callback) g_bang_callback(recv);
}

// ── Init ─────────────────────────────────────────────────────────────────────

// Only this function initializes libpd audio (zero inputs) and turns DSP on.
// Returns 0 on success; identical re-init is a no-op, different parameters return -1.
EXPORT int pd_bridge_init(int sample_rate, int channels) {
    auto &runtime = pdRuntime();
    std::lock_guard<std::mutex> lock(runtime.mutex);
    if (sample_rate <= 0 || channels <= 0) return -1;
    if (runtime.renderer) {
        return runtime.sampleRate == static_cast<unsigned int>(sample_rate) &&
               runtime.outChannels == static_cast<unsigned int>(channels) ? 0 : -1;
    }
    if (!runtime.libpdInitialized) {
        const int result = libpd_init();
        if (result != 0) return result;
        runtime.libpdInitialized = true;
    }
    auto renderer = std::make_unique<PdRenderer>(channels, libpd_blocksize(),
        [](float *output) {
            libpd_process_float(1, nullptr, output);
        }, runtime.mutex);
    int result = libpd_init_audio(0, channels, sample_rate);
    if (result != 0) return result;
    libpd_set_floathook(on_float);
    libpd_set_banghook(on_bang);
    // Turn DSP on
    result = libpd_start_message(1);
    if (result != 0) return result;
    libpd_add_float(1.0f);
    result = libpd_finish_message("pd", "dsp");
    if (result != 0) return result;
    runtime.sampleRate = sample_rate;
    runtime.outChannels = channels;
    runtime.renderer = std::move(renderer);
    return 0;
}

// ── Patch lifecycle ───────────────────────────────────────────────────────────

// Opens a .pd patch file, tracked by patch_id.
// Returns 0 on success, 1 if already open with that ID, -1 on failure.
EXPORT int pd_open_patch(const char* patch_id, const char* dir, const char* filename) {
    std::lock_guard<std::mutex> lock(pdRuntime().mutex);
    if (!pdRuntime().renderer || !patch_id || !dir || !filename) return -1;
    std::string id(patch_id);
    if (g_patches.count(id)) return 1;
    void* handle = libpd_openfile(filename, dir);
    if (!handle) return -1;
    g_patches[id] = handle;
    return 0;
}

// Opens a patch and routes its output in one step: sends `set <first+1> ... <first+channels>` to the
// patch's `$0-dac` receiver while the shared mutex is held, so no audio block runs with the patch on
// its default dac~ channels. first_channel is zero-based; Pd channel numbers are one-based.
// Returns 0 on success, 1 if already open with that ID, -1 on failure, -2 if the patch has no
// `$0-dac` receiver (the patch is closed again).
EXPORT int pd_open_patch_routed(const char* patch_id, const char* dir, const char* filename,
                                int first_channel, int channels) {
    auto &runtime = pdRuntime();
    std::lock_guard<std::mutex> lock(runtime.mutex);
    if (!runtime.renderer || !patch_id || !dir || !filename) return -1;
    if (first_channel < 0 || channels <= 0 ||
        static_cast<unsigned int>(first_channel + channels) > runtime.outChannels) return -1;
    std::string id(patch_id);
    if (g_patches.count(id)) return 1;
    void* handle = libpd_openfile(filename, dir);
    if (!handle) return -1;
    const std::string receiver = std::to_string(libpd_getdollarzero(handle)) + "-dac";
    if (libpd_start_message(channels) != 0) {
        libpd_closefile(handle);
        return -1;
    }
    for (int i = 0; i < channels; ++i) {
        libpd_add_float(static_cast<float>(first_channel + i + 1));
    }
    if (libpd_finish_message(receiver.c_str(), "set") != 0) {
        libpd_closefile(handle);
        return -2;
    }
    g_patches[id] = handle;
    return 0;
}

// Returns the stored patch handle's $0, or -1 for an unknown or closed patch ID.
EXPORT int pd_get_dollar_zero(const char* patch_id) {
    std::lock_guard<std::mutex> lock(pdRuntime().mutex);
    if (!patch_id) return -1;
    const auto it = g_patches.find(patch_id);
    return it == g_patches.end() ? -1 : libpd_getdollarzero(it->second);
}

// Closes the patch identified by patch_id. No-op if not found.
EXPORT void pd_close_patch(const char* patch_id) {
    std::lock_guard<std::mutex> lock(pdRuntime().mutex);
    if (!patch_id) return;
    auto it = g_patches.find(std::string(patch_id));
    if (it != g_patches.end()) {
        libpd_closefile(it->second);
        g_patches.erase(it);
    }
}

// Closes all open patches.
EXPORT void pd_close_all_patches() {
    std::lock_guard<std::mutex> lock(pdRuntime().mutex);
    for (auto& kv : g_patches) {
        libpd_closefile(kv.second);
    }
    g_patches.clear();
}

// ── Messaging: Dart → PD ─────────────────────────────────────────────────────
// Note: PD receivers are global across ALL loaded patches.
// There is no per-patch targeting — the receiver name determines which
// [receive] objects in which patches respond.

// Use a patch's $0 from pd_get_dollar_zero (Dart getDollarZero) for unique names.
// The shared mutex serializes messaging, including assembly, with audio and patches.

// Send a float value to a named receiver. Returns 0 on success.
EXPORT int pd_send_float(const char* receiver, float value) {
    std::lock_guard<std::mutex> lock(pdRuntime().mutex);
    if (!pdRuntime().renderer || !receiver) return -1;
    return libpd_float(receiver, value);
}

// Send a bang to a named receiver. Returns 0 on success.
EXPORT int pd_send_bang(const char* receiver) {
    std::lock_guard<std::mutex> lock(pdRuntime().mutex);
    if (!pdRuntime().renderer || !receiver) return -1;
    return libpd_bang(receiver);
}

// Send a symbol to a named receiver. Returns 0 on success.
EXPORT int pd_send_symbol(const char* receiver, const char* symbol) {
    std::lock_guard<std::mutex> lock(pdRuntime().mutex);
    if (!pdRuntime().renderer || !receiver || !symbol) return -1;
    return libpd_symbol(receiver, symbol);
}

// Send a list of floats to a named receiver. Returns 0 on success.
// libpd_start_message allocates the argument buffer and can fail — bail out
// before touching libpd_add_* if it does.
EXPORT int pd_send_list(const char* receiver, const float* values, int count) {
    std::lock_guard<std::mutex> lock(pdRuntime().mutex);
    if (!pdRuntime().renderer || !receiver) return -1;
    if (count < 0 || (count > 0 && !values)) return -1;
    int result = libpd_start_message(count);
    if (result != 0) return result;
    for (int i = 0; i < count; ++i) {
        libpd_add_float(values[i]);
    }
    return libpd_finish_list(receiver);
}

// Send a typed message (selector + float args) to a named receiver, i.e. the
// equivalent of `[; receiver selector arg1 arg2(`. Returns 0 on success.
EXPORT int pd_send_message(const char* receiver, const char* selector,
                           const float* values, int count) {
    std::lock_guard<std::mutex> lock(pdRuntime().mutex);
    if (!pdRuntime().renderer || !receiver || !selector) return -1;
    if (count < 0 || (count > 0 && !values)) return -1;
    int result = libpd_start_message(count);
    if (result != 0) return result;
    for (int i = 0; i < count; ++i) {
        libpd_add_float(values[i]);
    }
    return libpd_finish_message(receiver, selector);
}

// ── Callbacks: PD → Dart ─────────────────────────────────────────────────────

// Register a Dart NativeCallable to receive float outputs from patches.
EXPORT void pd_set_float_hook(void (*callback)(const char* recv, float val)) {
    std::lock_guard<std::mutex> lock(pdRuntime().mutex);
    g_float_callback = callback;
}

// Register a Dart NativeCallable to receive bang outputs from patches.
EXPORT void pd_set_bang_hook(void (*callback)(const char* recv)) {
    std::lock_guard<std::mutex> lock(pdRuntime().mutex);
    g_bang_callback = callback;
}
