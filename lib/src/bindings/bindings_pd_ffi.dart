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
        _pdOpenPatchRouted = lookup<
                ffi.NativeFunction<
                    ffi.Int32 Function(ffi.Pointer<Utf8>, ffi.Pointer<Utf8>,
                        ffi.Pointer<Utf8>, ffi.Int32, ffi.Int32)>>(
                'pd_open_patch_routed')
            .asFunction(),
        _pdGetDollarZero = lookup<
                ffi.NativeFunction<
                    ffi.Int32 Function(ffi.Pointer<Utf8>)>>(
                'pd_get_dollar_zero')
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
        _pdSendSymbol = lookup<
                ffi.NativeFunction<
                    ffi.Int32 Function(ffi.Pointer<Utf8>,
                        ffi.Pointer<Utf8>)>>('pd_send_symbol')
            .asFunction(),
        _pdSendList = lookup<
                ffi.NativeFunction<
                    ffi.Int32 Function(ffi.Pointer<Utf8>,
                        ffi.Pointer<ffi.Float>, ffi.Int32)>>('pd_send_list')
            .asFunction(),
        _pdSendMessage = lookup<
                ffi.NativeFunction<
                    ffi.Int32 Function(
                        ffi.Pointer<Utf8>,
                        ffi.Pointer<Utf8>,
                        ffi.Pointer<ffi.Float>,
                        ffi.Int32)>>('pd_send_message')
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
  final int Function(
      ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, int, int)
      _pdOpenPatchRouted;
  final int Function(ffi.Pointer<Utf8>) _pdGetDollarZero;
  final void Function(ffi.Pointer<Utf8>) _pdClosePatch;
  final void Function() _pdCloseAllPatches;
  final int Function(ffi.Pointer<Utf8>, double) _pdSendFloat;
  final int Function(ffi.Pointer<Utf8>) _pdSendBang;
  final int Function(ffi.Pointer<Utf8>, ffi.Pointer<Utf8>) _pdSendSymbol;
  final int Function(ffi.Pointer<Utf8>, ffi.Pointer<ffi.Float>, int)
      _pdSendList;
  final int Function(
          ffi.Pointer<Utf8>, ffi.Pointer<Utf8>, ffi.Pointer<ffi.Float>, int)
      _pdSendMessage;
  final void Function(
      ffi.Pointer<
          ffi.NativeFunction<
              ffi.Void Function(ffi.Pointer<Utf8>, ffi.Float)>>) _pdSetFloatHook;
  final void Function(
          ffi.Pointer<
              ffi.NativeFunction<ffi.Void Function(ffi.Pointer<Utf8>)>>)
      _pdSetBangHook;

  /// Initialize libpd audio and DSP. Identical re-init is a no-op;
  /// different parameters return -1. Returns 0 on success.
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

  /// Open a PD patch and route its `$0-dac` output to [channels] channels
  /// starting at zero-based [firstChannel], atomically with the open.
  /// Returns 0 on success, 1 if already open, -1 on failure, -2 if the patch
  /// has no `$0-dac` receiver.
  int openPatchRouted(String patchId, String dir, String filename,
      int firstChannel, int channels) {
    final cId = patchId.toNativeUtf8();
    final cDir = dir.toNativeUtf8();
    final cFile = filename.toNativeUtf8();
    try {
      return _pdOpenPatchRouted(cId, cDir, cFile, firstChannel, channels);
    } finally {
      calloc.free(cId);
      calloc.free(cDir);
      calloc.free(cFile);
    }
  }

  /// Returns the patch's $0, or -1 if [patchId] is unknown or closed.
  int getDollarZero(String patchId) {
    final cId = patchId.toNativeUtf8();
    try {
      return _pdGetDollarZero(cId);
    } finally {
      calloc.free(cId);
    }
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

  /// Send a symbol to a named PD receiver. Returns 0 on success.
  int sendSymbol(String receiver, String symbol) {
    final cRecv = receiver.toNativeUtf8();
    final cSym = symbol.toNativeUtf8();
    final result = _pdSendSymbol(cRecv, cSym);
    calloc.free(cRecv);
    calloc.free(cSym);
    return result;
  }

  /// Send a list of floats to a named PD receiver. Returns 0 on success.
  int sendList(String receiver, List<double> values) {
    final cRecv = receiver.toNativeUtf8();
    final cValues = _allocFloats(values);
    try {
      return _pdSendList(cRecv, cValues, values.length);
    } finally {
      calloc.free(cRecv);
      if (cValues != ffi.nullptr) calloc.free(cValues);
    }
  }

  /// Send a typed message (selector + float args) to a named PD receiver.
  /// Returns 0 on success.
  int sendMessage(String receiver, String selector, List<double> values) {
    final cRecv = receiver.toNativeUtf8();
    final cSel = selector.toNativeUtf8();
    final cValues = _allocFloats(values);
    try {
      return _pdSendMessage(cRecv, cSel, cValues, values.length);
    } finally {
      calloc.free(cRecv);
      calloc.free(cSel);
      if (cValues != ffi.nullptr) calloc.free(cValues);
    }
  }

  /// Allocates a native float array holding [values]. Returns `nullptr` for an
  /// empty list — the native side accepts a null pointer when count is 0.
  /// The caller owns the memory and must free it.
  static ffi.Pointer<ffi.Float> _allocFloats(List<double> values) {
    if (values.isEmpty) return ffi.nullptr;
    final ptr = calloc<ffi.Float>(values.length);
    for (var i = 0; i < values.length; i++) {
      ptr[i] = values[i];
    }
    return ptr;
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
