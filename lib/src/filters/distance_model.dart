// ignore_for_file: public_member_api_docs

import 'dart:math' as math;

class DistanceResult {
  final double directGain;
  final double reverbSend;
  final double lpCutoffHz;
  const DistanceResult({
    required this.directGain,
    required this.reverbSend,
    required this.lpCutoffHz,
  });
}

class DistanceModel {
  final EnvironmentPreset environment;

  DistanceModel(this.environment);

  DistanceResult compute(double distance) {
    final d = distance.clamp(0.01, 200.0);
    final ref = environment.referenceDistance;

    final directGain = d <= ref
        ? 1.0
        : (ref / (ref + environment.rolloffFactor * (d - ref))).clamp(0.0, 1.0);

    // Reverb fades slower than direct (lower rolloff factor), then scaled by
    // the environment's natural reverb-to-direct ratio at the reference distance.
    final reverbSend = d <= ref
        ? environment.reverbRelativeLevel
        : (ref / (ref + environment.reverbRolloffFactor * (d - ref)))
                .clamp(0.0, 1.0) *
            environment.reverbRelativeLevel;

    final lpCutoff = (20000.0 * math.exp(-d * environment.absorptionRate))
        .clamp(environment.minLpCutoff, 20000.0);

    return DistanceResult(
      directGain: directGain,
      reverbSend: reverbSend, // already incorporates reverbRelativeLevel
      lpCutoffHz: lpCutoff,
    );
  }
}

class EnvironmentPreset {
  final String name;
  final double referenceDistance;
  final double rolloffFactor;
  final double reverbRelativeLevel;
  final double reverbRolloffFactor;
  final double absorptionRate;
  final double minLpCutoff;

  const EnvironmentPreset({
    required this.name,
    required this.referenceDistance,
    required this.rolloffFactor,
    required this.reverbRelativeLevel,
    required this.reverbRolloffFactor,
    required this.absorptionRate,
    required this.minLpCutoff,
  });
}
