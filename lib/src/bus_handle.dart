import 'package:flutter_soloud/src/sound_handle.dart';

/// A handle for a SoLoud bus.
///
/// SoLoud buses are [AudioSource]s that act as submixes.
/// This handle is used to control bus-level properties (volume, filters, etc.)
/// and to play sounds through the bus.
///
/// It wraps a raw integer handle (the same type as [SoundHandle]) but provides
/// type safety to distinguish between a bus and a regular sound voice.
extension type const BusHandle._(int id) {
  /// Constructs a valid bus handle with [id].
  const BusHandle(this.id);

  /// Constructs an invalid bus handle.
  const BusHandle.error() : this._(-1);

  /// Checks if the handle represents an error.
  bool get isError => id < 0;

  /// Converts this bus handle to a [SoundHandle].
  ///
  /// Since a bus is played on the main engine as a voice, its handle
  /// is compatible with all methods taking a [SoundHandle] (volume, pause, etc.).
  SoundHandle get asSoundHandle => SoundHandle(id);
}
