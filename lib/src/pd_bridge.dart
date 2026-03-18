import 'package:flutter_soloud/src/bindings/bindings_pd_ffi.dart';
import 'package:flutter_soloud/src/bindings/soloud_controller.dart';
import 'package:logging/logging.dart';

/// Bridge to libpd for patch lifecycle and parameter messaging.
///
/// This class provides FFI access to libpd's patch open/close and
/// messaging functions. Audio processing is handled separately by
/// SoLoud's LibPDAudioSource (via `libpd_process_float()` on the
/// audio thread).
///
/// Usage:
/// ```dart
/// final pd = PdBridge.instance;
/// pd.init(sampleRate: 44100, channels: 2);
/// pd.openPatch(patchId: 'alpha', dir: '/path/to/', filename: 'alpha_waves.pd');
/// pd.sendFloat(receiver: 'freq', value: 440.0);
/// pd.closePatch(patchId: 'alpha');
/// ```
class PdBridge {
  PdBridge._() {
    _ffi = PdBridgeFfi.fromLookup(SoLoudController().nativeLib.lookup);
  }

  static final PdBridge instance = PdBridge._();

  static final _log = Logger('PdBridge');

  late final PdBridgeFfi _ffi;
  bool _initialized = false;

  /// Whether libpd has been initialized.
  bool get isInitialized => _initialized;

  /// Initialize libpd with the given sample rate and channel count.
  /// Must be called before opening patches. Safe to call multiple times
  /// (only the first call has effect).
  ///
  /// Returns true on success.
  bool init({required int sampleRate, required int channels}) {
    final result = _ffi.init(sampleRate, channels);
    if (result != 0) {
      _log.severe('pd_bridge_init failed with code $result');
      return false;
    }
    _initialized = true;
    _log.info('libpd initialized: ${sampleRate}Hz, $channels channels');
    return true;
  }

  /// Open a PD patch file, tracked by [patchId].
  ///
  /// [patchId] is an arbitrary string key used to reference this patch
  /// for closing later (e.g. 'binaural_alpha', 'rain_ambience').
  ///
  /// [dir] is the absolute directory path containing the patch file.
  /// [filename] is the patch filename (e.g. 'alpha_waves.pd').
  ///
  /// Returns true on success, false if the patch could not be opened.
  /// Returns false if a patch with this ID is already open.
  bool openPatch({
    required String patchId,
    required String dir,
    required String filename,
  }) {
    final result = _ffi.openPatch(patchId, dir, filename);
    if (result == 0) {
      _log.fine('Opened patch "$patchId": $dir/$filename');
      return true;
    }
    if (result == 1) {
      _log.warning('Patch "$patchId" is already open');
    } else {
      _log.severe('Failed to open patch "$patchId": $dir/$filename');
    }
    return false;
  }

  /// Close the patch identified by [patchId]. No-op if not found.
  void closePatch({required String patchId}) {
    _ffi.closePatch(patchId);
    _log.fine('Closed patch "$patchId"');
  }

  /// Close all open patches.
  void closeAllPatches() {
    _ffi.closeAllPatches();
    _log.fine('Closed all patches');
  }

  /// Send a float value to a named PD receiver.
  ///
  /// **Note:** PD receivers are global across ALL loaded patches. Any
  /// `[receive <name>]` object in any loaded patch will receive this
  /// message. There is no per-patch targeting — use unique receiver
  /// names in your patches to avoid conflicts.
  ///
  /// Returns true on success.
  bool sendFloat({required String receiver, required double value}) {
    return _ffi.sendFloat(receiver, value) == 0;
  }

  /// Send a bang to a named PD receiver.
  ///
  /// **Note:** PD receivers are global across ALL loaded patches.
  /// See [sendFloat] for details.
  ///
  /// Returns true on success.
  bool sendBang({required String receiver}) {
    return _ffi.sendBang(receiver) == 0;
  }
}
