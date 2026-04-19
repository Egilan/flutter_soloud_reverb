// ignore_for_file: public_member_api_docs

import 'package:flutter_soloud/src/filters/filters.dart';
import 'package:flutter_soloud/src/sound_handle.dart';
import 'package:flutter_soloud/src/sound_hash.dart';

enum DattorroEnum {
  preDelay,
  decay,
  damping,
  wet,
  dry,
  bandwidth,
  inputDiffusion,
  lfoRate,
  lfoDepth;

  // ignore: long-parameter-list
  final List<double> _mins = const [
    0.0, 0.0, 0.0, 0.0, 0.0, // preDelay, decay, damping, wet, dry
    0.0, 0.0, 0.1, 0.0,       // bandwidth, inputDiffusion, lfoRate, lfoDepth
  ];
  final List<double> _maxs = const [
    1.0, 0.99, 0.99, 1.0, 1.0, // preDelay, decay, damping, wet, dry
    1.0, 1.0, 10.0, 1.0,        // bandwidth, inputDiffusion, lfoRate, lfoDepth
  ];
  final List<double> _defs = const [
    0.0, 0.7, 0.5, 0.8, 0.5, // preDelay, decay, damping, wet, dry
    1.0, 1.0, 1.0, 1.0,       // bandwidth, inputDiffusion, lfoRate, lfoDepth
  ];

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
        DattorroEnum.bandwidth => 'Bandwidth',
        DattorroEnum.inputDiffusion => 'Input Diffusion',
        DattorroEnum.lfoRate => 'LFO Rate',
        DattorroEnum.lfoDepth => 'LFO Depth',
      };
}

abstract class _DattorroInternal extends FilterBase {
  const _DattorroInternal(SoundHash? soundHash)
      : super(FilterType.dattorroFilter, soundHash, null);

  DattorroEnum get queryPreDelay => DattorroEnum.preDelay;
  DattorroEnum get queryDecay => DattorroEnum.decay;
  DattorroEnum get queryDamping => DattorroEnum.damping;
  DattorroEnum get queryWet => DattorroEnum.wet;
  DattorroEnum get queryDry => DattorroEnum.dry;
  DattorroEnum get queryBandwidth => DattorroEnum.bandwidth;
  DattorroEnum get queryInputDiffusion => DattorroEnum.inputDiffusion;
  DattorroEnum get queryLfoRate => DattorroEnum.lfoRate;
  DattorroEnum get queryLfoDepth => DattorroEnum.lfoDepth;
}

class DattorroSingle extends _DattorroInternal {
  DattorroSingle(super.soundHash);

  FilterParam preDelay({SoundHandle? soundHandle}) => FilterParam(
        soundHandle,
        null,
        filterType,
        DattorroEnum.preDelay.index,
        DattorroEnum.preDelay.min,
        DattorroEnum.preDelay.max,
      );

  FilterParam decay({SoundHandle? soundHandle}) => FilterParam(
        soundHandle,
        null,
        filterType,
        DattorroEnum.decay.index,
        DattorroEnum.decay.min,
        DattorroEnum.decay.max,
      );

  FilterParam damping({SoundHandle? soundHandle}) => FilterParam(
        soundHandle,
        null,
        filterType,
        DattorroEnum.damping.index,
        DattorroEnum.damping.min,
        DattorroEnum.damping.max,
      );

  FilterParam wet({SoundHandle? soundHandle}) => FilterParam(
        soundHandle,
        null,
        filterType,
        DattorroEnum.wet.index,
        DattorroEnum.wet.min,
        DattorroEnum.wet.max,
      );

  FilterParam dry({SoundHandle? soundHandle}) => FilterParam(
        soundHandle,
        null,
        filterType,
        DattorroEnum.dry.index,
        DattorroEnum.dry.min,
        DattorroEnum.dry.max,
      );

  FilterParam bandwidth({SoundHandle? soundHandle}) => FilterParam(
        soundHandle,
        null,
        filterType,
        DattorroEnum.bandwidth.index,
        DattorroEnum.bandwidth.min,
        DattorroEnum.bandwidth.max,
      );

  FilterParam inputDiffusion({SoundHandle? soundHandle}) => FilterParam(
        soundHandle,
        null,
        filterType,
        DattorroEnum.inputDiffusion.index,
        DattorroEnum.inputDiffusion.min,
        DattorroEnum.inputDiffusion.max,
      );

  FilterParam lfoRate({SoundHandle? soundHandle}) => FilterParam(
        soundHandle,
        null,
        filterType,
        DattorroEnum.lfoRate.index,
        DattorroEnum.lfoRate.min,
        DattorroEnum.lfoRate.max,
      );

  FilterParam lfoDepth({SoundHandle? soundHandle}) => FilterParam(
        soundHandle,
        null,
        filterType,
        DattorroEnum.lfoDepth.index,
        DattorroEnum.lfoDepth.min,
        DattorroEnum.lfoDepth.max,
      );
}

class DattorroGlobal extends _DattorroInternal {
  const DattorroGlobal() : super(null);

  FilterParam get preDelay => FilterParam(
        null,
        null,
        filterType,
        DattorroEnum.preDelay.index,
        DattorroEnum.preDelay.min,
        DattorroEnum.preDelay.max,
      );

  FilterParam get decay => FilterParam(
        null,
        null,
        filterType,
        DattorroEnum.decay.index,
        DattorroEnum.decay.min,
        DattorroEnum.decay.max,
      );

  FilterParam get damping => FilterParam(
        null,
        null,
        filterType,
        DattorroEnum.damping.index,
        DattorroEnum.damping.min,
        DattorroEnum.damping.max,
      );

  FilterParam get wet => FilterParam(
        null,
        null,
        filterType,
        DattorroEnum.wet.index,
        DattorroEnum.wet.min,
        DattorroEnum.wet.max,
      );

  FilterParam get dry => FilterParam(
        null,
        null,
        filterType,
        DattorroEnum.dry.index,
        DattorroEnum.dry.min,
        DattorroEnum.dry.max,
      );

  FilterParam get bandwidth => FilterParam(
        null,
        null,
        filterType,
        DattorroEnum.bandwidth.index,
        DattorroEnum.bandwidth.min,
        DattorroEnum.bandwidth.max,
      );

  FilterParam get inputDiffusion => FilterParam(
        null,
        null,
        filterType,
        DattorroEnum.inputDiffusion.index,
        DattorroEnum.inputDiffusion.min,
        DattorroEnum.inputDiffusion.max,
      );

  FilterParam get lfoRate => FilterParam(
        null,
        null,
        filterType,
        DattorroEnum.lfoRate.index,
        DattorroEnum.lfoRate.min,
        DattorroEnum.lfoRate.max,
      );

  FilterParam get lfoDepth => FilterParam(
        null,
        null,
        filterType,
        DattorroEnum.lfoDepth.index,
        DattorroEnum.lfoDepth.min,
        DattorroEnum.lfoDepth.max,
      );
}
