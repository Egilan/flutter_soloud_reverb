import 'dart:developer' as dev;

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter_soloud/flutter_soloud.dart';
import 'package:logging/logging.dart';

void main() async {
  // Standard logging setup
  Logger.root.level = kDebugMode ? Level.FINE : Level.INFO;
  Logger.root.onRecord.listen((record) {
    dev.log(
      record.message,
      time: record.time,
      level: record.level.value,
      name: record.loggerName,
      zone: record.zone,
      error: record.error,
      stackTrace: record.stackTrace,
    );
  });

  WidgetsFlutterBinding.ensureInitialized();

  /// Initialize the player.
  await SoLoud.instance.init();

  runApp(
    const MaterialApp(
      home: HelloFlutterSoLoud(),
    ),
  );
}

class HelloFlutterSoLoud extends StatefulWidget {
  const HelloFlutterSoLoud({super.key});

  @override
  State<HelloFlutterSoLoud> createState() => _HelloFlutterSoLoudState();
}

class _HelloFlutterSoLoudState extends State<HelloFlutterSoLoud> {
  // The asset to load
  AudioSource? currentSound;

  // The specific instance of the playing sound (needed to tweak params live)
  SoundHandle? currentHandle;

  // Reverb Parameters
  double feedback = 0.20; // Default for ReverbSc
  double wet = 1.0;       // Fully wet
  double dry = 0.7;       // Mix in some dry signal
  bool enableReverb = true;

  @override
  void dispose() {
    SoLoud.instance.deinit();
    super.dispose();
  }

  /// Loads the asset and sets up the filter
  Future<void> _setupSound() async {
    // 1. Cleanup previous
    await SoLoud.instance.disposeAllSources();

    // 2. Load the file
    if (kIsWeb) {
      currentSound = await SoLoud.instance.loadAsset(
        'assets/audio/8_bit_mentality.mp3',
        mode: LoadMode.disk,
      );
    } else {
      currentSound = await SoLoud.instance
          .loadAsset('assets/audio/speech_20250803074809927_01.mp3');
    }

    // 3. IMPORTANT: Add the ReverbSc filter to this sound source.
    // We do this *before* playing.
    if (currentSound != null) {
      currentSound!.filters.reverbScFilter.activate();

      // Apply initial values
      _applyReverbParams(null); // null handle = apply to Source defaults
    }
  }

  /// Plays the sound
  Future<void> _play() async {
    // 1. If something is already playing, stop it first!
    if (currentHandle != null) {
      await SoLoud.instance.stop(currentHandle!);
    }

    if (currentSound == null) await _setupSound();

    if (currentSound != null) {
      // 2. Play the new sound
      currentHandle = await SoLoud.instance.play(currentSound!);

      // 3. Immediately sync the sliders to this new handle
      // (Otherwise it starts with defaults until you touch a slider)
      _applyReverbParams(currentHandle);

      setState(() {});
    }
  }

  /// Stop playback
  Future<void> _stop() async {
    if (currentHandle != null) {
      await SoLoud.instance.stop(currentHandle!);
      currentHandle = null;
      setState(() {});
    }
  }

  /// Updates parameters on the live sound
  void _applyReverbParams(SoundHandle? handle) {
    if (currentSound == null) return;

    // We can verify the filter is active first
    final int index = currentSound!.filters.reverbScFilter.index;
    if (index == -1) return; // Filter not active

    // Update Feedback
    currentSound!.filters.reverbScFilter.feedback(soundHandle: handle).value = feedback;

    // Update Wet/Dry
    currentSound!.filters.reverbScFilter.wet(soundHandle: handle).value = wet;
    currentSound!.filters.reverbScFilter.dry(soundHandle: handle).value = dry;
  }

  @override
  Widget build(BuildContext context) {
    if (!SoLoud.instance.isInitialized) return const SizedBox.shrink();

    return Scaffold(
      appBar: AppBar(title: const Text("ReverbSc Test")),
      body: Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            // --- Controls ---
            Row(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                ElevatedButton.icon(
                  onPressed: _play,
                  icon: const Icon(Icons.play_arrow),
                  label: const Text('Play'),
                ),
                const SizedBox(width: 20),
                ElevatedButton.icon(
                  onPressed: _stop,
                  icon: const Icon(Icons.stop),
                  label: const Text('Stop'),
                ),
              ],
            ),

            const SizedBox(height: 40),
            const Divider(),
            const Text("Reverb Parameters (Tweaking Live)", style: TextStyle(fontWeight: FontWeight.bold)),

            // --- Feedback Slider ---
            Padding(
              padding: const EdgeInsets.symmetric(horizontal: 20),
              child: Row(
                children: [
                  const Text("Feedback (Tail)"),
                  Expanded(
                    child: Slider(
                      value: feedback,
                      min: 0.0,
                      max: 0.8, // Don't go to 1.0 or it might self-oscillate forever
                      onChanged: (val) {
                        setState(() => feedback = val);
                        _applyReverbParams(currentHandle);
                      },
                    ),
                  ),
                  Text(feedback.toStringAsFixed(2)),
                ],
              ),
            ),

            // --- Wet Slider ---
            Padding(
              padding: const EdgeInsets.symmetric(horizontal: 20),
              child: Row(
                children: [
                  const Text("Wet (Reverb Vol)"),
                  Expanded(
                    child: Slider(
                      value: wet,
                      min: 0.0,
                      max: 1.0,
                      onChanged: (val) {
                        setState(() => wet = val);
                        _applyReverbParams(currentHandle);
                      },
                    ),
                  ),
                  Text(wet.toStringAsFixed(2)),
                ],
              ),
            ),

            // --- Dry Slider ---
            Padding(
              padding: const EdgeInsets.symmetric(horizontal: 20),
              child: Row(
                children: [
                  const Text("Dry (Original Vol)"),
                  Expanded(
                    child: Slider(
                      value: dry,
                      min: 0.0,
                      max: 1.0,
                      onChanged: (val) {
                        setState(() => dry = val);
                        _applyReverbParams(currentHandle);
                      },
                    ),
                  ),
                  Text(dry.toStringAsFixed(2)),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}
