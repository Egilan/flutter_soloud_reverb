// ignore_for_file: public_member_api_docs

import 'package:flutter_soloud/src/bindings/soloud_controller.dart';
import 'package:flutter_soloud/src/enums.dart';
import 'package:flutter_soloud/src/exceptions/exceptions.dart';
import 'package:flutter_soloud/src/filters/filters.dart';
import 'package:flutter_soloud/src/sound_handle.dart';
import 'package:flutter_soloud/src/sound_hash.dart';
import 'package:logging/logging.dart';

enum HrtfEnum {
  azimuth,
  elevation,
  wet,
  crossoverFreq;

  final List<double> _mins = const [-90.0, -90.0, 0.0, 20.0];
  final List<double> _maxs = const [90.0, 90.0, 1.0, 2000.0];
  final List<double> _defs = const [0.0, 0.0, 1.0, 150.0];

  double get min => _mins[index];
  double get max => _maxs[index];
  double get def => _defs[index];

  @override
  String toString() => switch (this) {
        HrtfEnum.azimuth => 'Azimuth',
        HrtfEnum.elevation => 'Elevation',
        HrtfEnum.wet => 'Wet',
        HrtfEnum.crossoverFreq => 'Crossover Freq',
      };
}

abstract class _HrtfInternal extends FilterBase {
  const _HrtfInternal(this.soundHash)
      : super(FilterType.hrtfFilter, soundHash);

  final SoundHash? soundHash;

  /// Load a KEMAR binary HRTF file from [path].
  /// The filter must be active before calling this.
  /// Throws [SoLoudCppException] on failure.
  void loadHrtfData(String path) {
    if (!isActive) {
      Logger('flutter_soloud.hrtfFilter')
          .severe(() => 'loadHrtfData: filter is not active');
      throw SoLoudCppException.fromPlayerError(PlayerErrors.filterNotFound);
    }
    final error = SoLoudController().soLoudFFI.loadHrtfData(
          soundHash: soundHash?.hash ?? 0,
          path: path,
        );
    if (error != PlayerErrors.noError) {
      Logger('flutter_soloud.hrtfFilter')
          .severe(() => 'loadHrtfData: $error');
      throw SoLoudCppException.fromPlayerError(error);
    }
  }
}

class HrtfSingle extends _HrtfInternal {
  HrtfSingle(super.soundHash);

  FilterParam azimuth({SoundHandle? soundHandle}) => FilterParam(
        soundHandle,
        filterType,
        HrtfEnum.azimuth.index,
        HrtfEnum.azimuth.min,
        HrtfEnum.azimuth.max,
      );

  FilterParam elevation({SoundHandle? soundHandle}) => FilterParam(
        soundHandle,
        filterType,
        HrtfEnum.elevation.index,
        HrtfEnum.elevation.min,
        HrtfEnum.elevation.max,
      );

  FilterParam wet({SoundHandle? soundHandle}) => FilterParam(
        soundHandle,
        filterType,
        HrtfEnum.wet.index,
        HrtfEnum.wet.min,
        HrtfEnum.wet.max,
      );

  FilterParam crossoverFreq({SoundHandle? soundHandle}) => FilterParam(
        soundHandle,
        filterType,
        HrtfEnum.crossoverFreq.index,
        HrtfEnum.crossoverFreq.min,
        HrtfEnum.crossoverFreq.max,
      );
}

class HrtfGlobal extends _HrtfInternal {
  const HrtfGlobal() : super(null);

  FilterParam get azimuth => FilterParam(
        null,
        filterType,
        HrtfEnum.azimuth.index,
        HrtfEnum.azimuth.min,
        HrtfEnum.azimuth.max,
      );

  FilterParam get elevation => FilterParam(
        null,
        filterType,
        HrtfEnum.elevation.index,
        HrtfEnum.elevation.min,
        HrtfEnum.elevation.max,
      );

  FilterParam get wet => FilterParam(
        null,
        filterType,
        HrtfEnum.wet.index,
        HrtfEnum.wet.min,
        HrtfEnum.wet.max,
      );

  FilterParam get crossoverFreq => FilterParam(
        null,
        filterType,
        HrtfEnum.crossoverFreq.index,
        HrtfEnum.crossoverFreq.min,
        HrtfEnum.crossoverFreq.max,
      );
}
