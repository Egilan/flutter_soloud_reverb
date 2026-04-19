// ignore_for_file: public_member_api_docs

import 'package:flutter_soloud/src/bindings/soloud_controller.dart';
import 'package:flutter_soloud/src/exceptions/exceptions.dart';
import 'package:flutter_soloud/src/enums.dart';
import 'package:flutter_soloud/src/filters/filters.dart';
import 'package:flutter_soloud/src/sound_handle.dart';
import 'package:flutter_soloud/src/sound_hash.dart';
import 'package:logging/logging.dart';

enum ConvolutionEnum {
  wet,
  dry;

  // Min/Max/Defs based on the C++ wrapper we wrote
  final List<double> _mins = const [0.0, 0.0];
  final List<double> _maxs = const [1.0, 1.0];
  final List<double> _defs = const [1.0, 0.5];

  double get min => _mins[index];
  double get max => _maxs[index];
  double get def => _defs[index];

  @override
  String toString() => switch (this) {
        ConvolutionEnum.wet => 'Wet',
        ConvolutionEnum.dry => 'Dry',
      };
}

abstract class _ConvolutionInternal extends FilterBase {
  const _ConvolutionInternal(this.soundHash)
      : super(FilterType.convolutionFilter, soundHash, null);

  final SoundHash? soundHash;

  ConvolutionEnum get queryWet => ConvolutionEnum.wet;
  ConvolutionEnum get queryDry => ConvolutionEnum.dry;

  /// Loads an Impulse Response (IR) audio file (e.g. WAV or OGG) to be used by the convolution filter.
  void loadIr(String path) {
    if (!isActive) {
      Logger('flutter_soloud.${filterType.name}Filter')
          .severe(() => 'loadIr: filter is not active');
      throw SoLoudCppException.fromPlayerError(PlayerErrors.filterNotFound);
    }
    final error = SoLoudController().soLoudFFI.loadConvolutionIR(
          soundHash: soundHash?.hash ?? 0,
          irPath: path,
        );
    if (error != PlayerErrors.noError) {
      Logger('flutter_soloud.${filterType.name}Filter')
          .severe(() => 'loadIr: $error');
      throw SoLoudCppException.fromPlayerError(error);
    }
  }
}

class ConvolutionSingle extends _ConvolutionInternal {
  ConvolutionSingle(super.soundHash);

  FilterParam wet({SoundHandle? soundHandle}) => FilterParam(
        soundHandle,
        null,
        filterType,
        ConvolutionEnum.wet.index,
        ConvolutionEnum.wet.min,
        ConvolutionEnum.wet.max,
      );

  FilterParam dry({SoundHandle? soundHandle}) => FilterParam(
        soundHandle,
        null,
        filterType,
        ConvolutionEnum.dry.index,
        ConvolutionEnum.dry.min,
        ConvolutionEnum.dry.max,
      );
}

class ConvolutionGlobal extends _ConvolutionInternal {
  const ConvolutionGlobal() : super(null);

  FilterParam get wet => FilterParam(
        null,
        null,
        filterType,
        ConvolutionEnum.wet.index,
        ConvolutionEnum.wet.min,
        ConvolutionEnum.wet.max,
      );

  FilterParam get dry => FilterParam(
        null,
        null,
        filterType,
        ConvolutionEnum.dry.index,
        ConvolutionEnum.dry.min,
        ConvolutionEnum.dry.max,
      );
}
