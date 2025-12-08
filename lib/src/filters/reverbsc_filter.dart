// ignore_for_file: public_member_api_docs

import 'package:flutter_soloud/src/filters/filters.dart';
import 'package:flutter_soloud/src/sound_handle.dart';
import 'package:flutter_soloud/src/sound_hash.dart';

enum ReverbScEnum {
  feedback,
  lpFreq,
  wet,
  dry;

  // Min/Max/Defs based on the C++ wrapper we wrote
  final List<double> _mins = const [0.0, 0.0, 0.0, 0.0];
  final List<double> _maxs = const [1.0, 22000.0, 1.0, 1.0];
  final List<double> _defs = const [0.85, 10000.0, 1.0, 0.7];

  double get min => _mins[index];
  double get max => _maxs[index];
  double get def => _defs[index];

  @override
  String toString() => switch (this) {
        ReverbScEnum.feedback => 'Feedback',
        ReverbScEnum.lpFreq => 'Low Pass Frequency',
        ReverbScEnum.wet => 'Wet',
        ReverbScEnum.dry => 'Dry',
      };
}

abstract class _ReverbScInternal extends FilterBase {
  const _ReverbScInternal(SoundHash? soundHash)
      : super(FilterType.reverbScFilter, soundHash);

  ReverbScEnum get queryFeedback => ReverbScEnum.feedback;
  ReverbScEnum get queryLpFreq => ReverbScEnum.lpFreq;
  ReverbScEnum get queryWet => ReverbScEnum.wet;
  ReverbScEnum get queryDry => ReverbScEnum.dry;
}

class ReverbScSingle extends _ReverbScInternal {
  ReverbScSingle(super.soundHash);

  FilterParam feedback({SoundHandle? soundHandle}) => FilterParam(
        soundHandle,
        filterType,
        ReverbScEnum.feedback.index,
        ReverbScEnum.feedback.min,
        ReverbScEnum.feedback.max,
      );

  FilterParam lpFreq({SoundHandle? soundHandle}) => FilterParam(
        soundHandle,
        filterType,
        ReverbScEnum.lpFreq.index,
        ReverbScEnum.lpFreq.min,
        ReverbScEnum.lpFreq.max,
      );

  FilterParam wet({SoundHandle? soundHandle}) => FilterParam(
        soundHandle,
        filterType,
        ReverbScEnum.wet.index,
        ReverbScEnum.wet.min,
        ReverbScEnum.wet.max,
      );

  FilterParam dry({SoundHandle? soundHandle}) => FilterParam(
        soundHandle,
        filterType,
        ReverbScEnum.dry.index,
        ReverbScEnum.dry.min,
        ReverbScEnum.dry.max,
      );
}

class ReverbScGlobal extends _ReverbScInternal {
  const ReverbScGlobal() : super(null);

  FilterParam get feedback => FilterParam(
        null,
        filterType,
        ReverbScEnum.feedback.index,
        ReverbScEnum.feedback.min,
        ReverbScEnum.feedback.max,
      );

  FilterParam get lpFreq => FilterParam(
        null,
        filterType,
        ReverbScEnum.lpFreq.index,
        ReverbScEnum.lpFreq.min,
        ReverbScEnum.lpFreq.max,
      );

  FilterParam get wet => FilterParam(
        null,
        filterType,
        ReverbScEnum.wet.index,
        ReverbScEnum.wet.min,
        ReverbScEnum.wet.max,
      );

  FilterParam get dry => FilterParam(
        null,
        filterType,
        ReverbScEnum.dry.index,
        ReverbScEnum.dry.min,
        ReverbScEnum.dry.max,
      );
}