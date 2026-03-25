// ignore_for_file: public_member_api_docs

import 'package:flutter_soloud/src/filters/filters.dart';
import 'package:flutter_soloud/src/sound_handle.dart';
import 'package:flutter_soloud/src/sound_hash.dart';

enum DattorroEnum {
  preDelay,
  decay,
  damping,
  wet,
  dry;

  final List<double> _mins = const [0.0, 0.0, 0.0, 0.0, 0.0];
  final List<double> _maxs = const [1.0, 0.99, 0.99, 1.0, 1.0];
  final List<double> _defs = const [0.0, 0.7, 0.5, 0.8, 0.5];

  double get min => _mins[index];
  double get max => _maxs[index];
  double get def => _defs[index];

  @override
  String toString() => switch (this) {
        DattorroEnum.preDelay => 'Pre-Delay',
        DattorroEnum.decay => 'Decay',
        DattorroEnum.damping => 'Damping',
        DattorroEnum.wet => 'Wet',
        DattorroEnum.dry => 'Dry',
      };
}

abstract class _DattorroInternal extends FilterBase {
  const _DattorroInternal(SoundHash? soundHash)
      : super(FilterType.dattorroFilter, soundHash);

  DattorroEnum get queryPreDelay => DattorroEnum.preDelay;
  DattorroEnum get queryDecay => DattorroEnum.decay;
  DattorroEnum get queryDamping => DattorroEnum.damping;
  DattorroEnum get queryWet => DattorroEnum.wet;
  DattorroEnum get queryDry => DattorroEnum.dry;
}

class DattorroSingle extends _DattorroInternal {
  DattorroSingle(super.soundHash);

  FilterParam preDelay({SoundHandle? soundHandle}) => FilterParam(
        soundHandle,
        filterType,
        DattorroEnum.preDelay.index,
        DattorroEnum.preDelay.min,
        DattorroEnum.preDelay.max,
      );

  FilterParam decay({SoundHandle? soundHandle}) => FilterParam(
        soundHandle,
        filterType,
        DattorroEnum.decay.index,
        DattorroEnum.decay.min,
        DattorroEnum.decay.max,
      );

  FilterParam damping({SoundHandle? soundHandle}) => FilterParam(
        soundHandle,
        filterType,
        DattorroEnum.damping.index,
        DattorroEnum.damping.min,
        DattorroEnum.damping.max,
      );

  FilterParam wet({SoundHandle? soundHandle}) => FilterParam(
        soundHandle,
        filterType,
        DattorroEnum.wet.index,
        DattorroEnum.wet.min,
        DattorroEnum.wet.max,
      );

  FilterParam dry({SoundHandle? soundHandle}) => FilterParam(
        soundHandle,
        filterType,
        DattorroEnum.dry.index,
        DattorroEnum.dry.min,
        DattorroEnum.dry.max,
      );
}

class DattorroGlobal extends _DattorroInternal {
  const DattorroGlobal() : super(null);

  FilterParam get preDelay => FilterParam(
        null,
        filterType,
        DattorroEnum.preDelay.index,
        DattorroEnum.preDelay.min,
        DattorroEnum.preDelay.max,
      );

  FilterParam get decay => FilterParam(
        null,
        filterType,
        DattorroEnum.decay.index,
        DattorroEnum.decay.min,
        DattorroEnum.decay.max,
      );

  FilterParam get damping => FilterParam(
        null,
        filterType,
        DattorroEnum.damping.index,
        DattorroEnum.damping.min,
        DattorroEnum.damping.max,
      );

  FilterParam get wet => FilterParam(
        null,
        filterType,
        DattorroEnum.wet.index,
        DattorroEnum.wet.min,
        DattorroEnum.wet.max,
      );

  FilterParam get dry => FilterParam(
        null,
        filterType,
        DattorroEnum.dry.index,
        DattorroEnum.dry.min,
        DattorroEnum.dry.max,
      );
}
