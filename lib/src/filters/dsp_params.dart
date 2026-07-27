import 'package:meta/meta.dart';

/// Parameters for the global sidechain ducker.
///
/// The ducker attenuates every bus except the key bus (and buses that opted
/// out) based on the key bus's signal envelope. All time values are in
/// seconds, levels in dB.
@immutable
class DuckerParams {
  /// Creates a set of ducker parameters.
  const DuckerParams({
    this.thresholdDb = -20,
    this.ratio = 2,
    this.kneeDb = 6,
    this.attackSec = 0.010,
    this.releaseSec = 0.250,
    this.maxDepthDb = 6,
  });

  /// Key envelope level (dBFS) above which ducking starts.
  final double thresholdDb;

  /// Downward compression ratio applied to the ducked buses (>= 1).
  final double ratio;

  /// Total soft-knee width in dB.
  final double kneeDb;

  /// Key envelope attack time in seconds.
  final double attackSec;

  /// Key envelope release time in seconds.
  final double releaseSec;

  /// Hard cap on attenuation in positive dB (ducked buses never disappear).
  final double maxDepthDb;

  /// Returns a copy with the given fields replaced.
  DuckerParams copyWith({
    double? thresholdDb,
    double? ratio,
    double? kneeDb,
    double? attackSec,
    double? releaseSec,
    double? maxDepthDb,
  }) {
    return DuckerParams(
      thresholdDb: thresholdDb ?? this.thresholdDb,
      ratio: ratio ?? this.ratio,
      kneeDb: kneeDb ?? this.kneeDb,
      attackSec: attackSec ?? this.attackSec,
      releaseSec: releaseSec ?? this.releaseSec,
      maxDepthDb: maxDepthDb ?? this.maxDepthDb,
    );
  }

  @override
  bool operator ==(Object other) =>
      other is DuckerParams &&
      other.thresholdDb == thresholdDb &&
      other.ratio == ratio &&
      other.kneeDb == kneeDb &&
      other.attackSec == attackSec &&
      other.releaseSec == releaseSec &&
      other.maxDepthDb == maxDepthDb;

  @override
  int get hashCode => Object.hash(
        thresholdDb,
        ratio,
        kneeDb,
        attackSec,
        releaseSec,
        maxDepthDb,
      );

  @override
  String toString() => 'DuckerParams(thresholdDb: $thresholdDb, '
      'ratio: $ratio, kneeDb: $kneeDb, attackSec: $attackSec, '
      'releaseSec: $releaseSec, maxDepthDb: $maxDepthDb)';
}

/// Parameters for the master brickwall limiter.
///
/// The limiter runs on the master mix, after the global filters and before
/// the output clip stage. It adds [lookaheadMs] of constant latency.
@immutable
class LimiterParams {
  /// Creates a set of limiter parameters.
  const LimiterParams({
    this.ceilingDb = -0.3,
    this.lookaheadMs = 3,
    this.releaseSec = 0.120,
  });

  /// Output ceiling in dBFS. Output never exceeds this level.
  final double ceilingDb;

  /// Lookahead time in milliseconds, clamped natively to [1, 5].
  ///
  /// Note: changing this resets the internal delay line (a single click may
  /// be audible); latency is otherwise constant, bypassed or not.
  final double lookaheadMs;

  /// Gain release time in seconds (typically 0.080-0.200).
  final double releaseSec;

  /// Returns a copy with the given fields replaced.
  LimiterParams copyWith({
    double? ceilingDb,
    double? lookaheadMs,
    double? releaseSec,
  }) {
    return LimiterParams(
      ceilingDb: ceilingDb ?? this.ceilingDb,
      lookaheadMs: lookaheadMs ?? this.lookaheadMs,
      releaseSec: releaseSec ?? this.releaseSec,
    );
  }

  @override
  bool operator ==(Object other) =>
      other is LimiterParams &&
      other.ceilingDb == ceilingDb &&
      other.lookaheadMs == lookaheadMs &&
      other.releaseSec == releaseSec;

  @override
  int get hashCode => Object.hash(ceilingDb, lookaheadMs, releaseSec);

  @override
  String toString() => 'LimiterParams(ceilingDb: $ceilingDb, '
      'lookaheadMs: $lookaheadMs, releaseSec: $releaseSec)';
}
