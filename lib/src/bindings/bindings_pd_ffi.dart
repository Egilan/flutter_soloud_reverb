import 'dart:ffi' as ffi;
import 'package:ffi/ffi.dart';

/// Low-level FFI bindings for pd_bridge.cpp exports.
class PdBridgeFfi {
  PdBridgeFfi.fromLookup(
    ffi.Pointer<T> Function<T extends ffi.NativeType>(String symbolName)
        lookup,
  )   : _pdBridgeInit = lookup<
                ffi.NativeFunction<
                    ffi.Int32 Function(ffi.Int32, ffi.Int32)>>('pd_bridge_init')
            .asFunction(),
        _pdOpenPatch = lookup<
                ffi.NativeFunction<
                    ffi.Int32 Function(ffi.Pointer<Utf8>, ffi.Pointer<Utf8>,
                        ffi.Pointer<Utf8>)>>('pd_open_patch')
            .asFunction(),
        _pdClosePatch = lookup<
                ffi.NativeFunction<
                    ffi.Void Function(ffi.Pointer<Utf8>)>>('pd_close_patch')
            .asFunction(),
        _pdCloseAllPatches =
            lookup<ffi.NativeFunction<ffi.Void Function()>>(
                    'pd_close_all_patches')
                .asFunction(),
        _pdSendFloat = lookup<
                ffi.NativeFunction<
                    ffi.Int32 Function(
                        ffi.Pointer<Utf8>, ffi.Float)>>('pd_send_float')
            .asFunction(),
        _pdSendBang = lookup<
                ffi.NativeFunction<
                    ffi.Int32 Function(ffi.Pointer<Utf8>)>>('pd_send_bang')
            .asFunction(),
        _pdSetFloatHook = lookup<
                ffi.NativeFunction<
                    ffi.Void Function(
                        ffi.Pointer<
                            ffi.NativeFunction<
                                ffi.Void Function(ffi.Pointer<Utf8>,
                                    ffi.Float)>>)>>('pd_set_float_hook')
            .asFunction(),
        _pdSetBangHook = lookup<
                ffi.NativeFunction<
                    ffi.Void Function(
                        ffi.Pointer<
                            ffi.NativeFunction<
                                ffi.Void Function(
                                    ffi.Pointer<Utf8>)>>)>>('pd_set_bang_hook')
            .asFunction();

  final int Function(int, int) _pdBridgeInit;
  final int Function(ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>)
      _pdOpenPatch;
  final void Function(ffi.Pointer<Utf8>) _pdClosePatch;
  final void Function() _pdCloseAllPatches;
  final int Function(ffi.Pointer<Utf8>, double) _pdSendFloat;
  final int Function(ffi.Pointer<Utf8>) _pdSendBang;
  final void Function(
      ffi.Pointer<
          ffi.NativeFunction<
              ffi.Void Function(ffi.Pointer<Utf8>, ffi.Float)>>) _pdSetFloatHook;
  final void Function(
          ffi.Pointer<
              ffi.NativeFunction<ffi.Void Function(ffi.Pointer<Utf8>)>>)
      _pdSetBangHook;

  /// Initialize libpd. Returns 0 on success.
  int init(int sampleRate, int channels) => _pdBridgeInit(sampleRate, channels);

  /// Open a PD patch tracked by [patchId].
  /// Returns 0 on success, 1 if already open, -1 on failure.
  int openPatch(String patchId, String dir, String filename) {
    final cId = patchId.toNativeUtf8();
    final cDir = dir.toNativeUtf8();
    final cFile = filename.toNativeUtf8();
    final result = _pdOpenPatch(cId, cDir, cFile);
    calloc.free(cId);
    calloc.free(cDir);
    calloc.free(cFile);
    return result;
  }

  /// Close the patch identified by [patchId].
  void closePatch(String patchId) {
    final cId = patchId.toNativeUtf8();
    _pdClosePatch(cId);
    calloc.free(cId);
  }

  /// Close all open patches.
  void closeAllPatches() => _pdCloseAllPatches();

  /// Send a float to a named PD receiver. Returns 0 on success.
  int sendFloat(String receiver, double value) {
    final cRecv = receiver.toNativeUtf8();
    final result = _pdSendFloat(cRecv, value);
    calloc.free(cRecv);
    return result;
  }

  /// Send a bang to a named PD receiver. Returns 0 on success.
  int sendBang(String receiver) {
    final cRecv = receiver.toNativeUtf8();
    final result = _pdSendBang(cRecv);
    calloc.free(cRecv);
    return result;
  }

  /// Set the native float hook callback.
  void setFloatHook(
      ffi.Pointer<
              ffi.NativeFunction<
                  ffi.Void Function(ffi.Pointer<Utf8>, ffi.Float)>>
          callback) {
    _pdSetFloatHook(callback);
  }

  /// Set the native bang hook callback.
  void setBangHook(
      ffi.Pointer<
              ffi.NativeFunction<ffi.Void Function(ffi.Pointer<Utf8>)>>
          callback) {
    _pdSetBangHook(callback);
  }
}
