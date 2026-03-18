# IR-Based Reverb Buses in Flutter Soloud

This document describes how to use Impulse Response (IR) based reverb effects in Flutter Soloud using audio buses.

## Overview

Flutter Soloud supports convolution-based reverb through the use of **buses** and the **convolution filter**. This approach allows you to:

- Apply different reverb effects to different audio sources
- Create submixes with independent reverb settings
- Load custom impulse response files (WAV/OGG) for realistic acoustic environments

## Key Concepts

### Audio Buses

An audio bus is a virtual mixing channel that acts as a submix. You can play sounds directly on a bus, and any filters attached to that bus will process those sounds. This is useful for applying effects like reverb to specific groups of sounds.

### Convolution Filter

The convolution filter (`FilterType.convolutionFilter`) applies an Impulse Response (IR) audio file to simulate acoustic spaces. The IR file contains the characteristics of a real acoustic environment (room size, reflections, etc.).

### Impulse Response (IR) Files

IR files are audio recordings that capture the acoustic characteristics of a space. They are typically:
- WAV or OGG format
- Short recordings (typically 0.5-5 seconds)
- Can be found online from various free sources or created from real spaces

## API Reference

### Bus Management

#### `SoLoud.instance.createBus()`

Creates a new audio bus and returns a `BusHandle`.

```dart
final bus = SoLoud.instance.createBus();
```

#### `SoLoud.instance.destroyBus(BusHandle bus)`

Destroys a previously created bus.

```dart
SoLoud.instance.destroyBus(bus);
```

### Filter Management

#### `SoLoud.instance.addBusFilter(BusHandle bus, FilterType filterType)`

Adds a filter to a bus. For IR-based reverb, add the convolution filter:

```dart
SoLoud.instance.addBusFilter(bus, FilterType.convolutionFilter);
```

#### `SoLoud.instance.removeBusFilter(BusHandle bus, FilterType filterType)`

Removes a filter from a bus:

```dart
SoLoud.instance.removeBusFilter(bus, FilterType.convolutionFilter);
```

### Loading IR Files

#### `SoLoud.instance.loadBusConvolutionIR(BusHandle bus, String irPath)`

Loads an Impulse Response audio file onto a bus's convolution filter:

```dart
SoLoud.instance.loadBusConvolutionIR(bus, 'assets/audio/IR/my_reverb.wav');
```

### Playback

#### `SoLoud.instance.playOnBus(BusHandle bus, AudioSource sound, {double volume, double pan, bool paused, bool looping})`

Plays a loaded sound on a specific bus:

```dart
final handle = await SoLoud.instance.playOnBus(bus, soundSource);
```

### Volume Control

#### `SoLoud.instance.setBusVolume(BusHandle bus, double volume)`

Sets the volume of a bus (0.0 to any value, 1.0 is normal):

```dart
SoLoud.instance.setBusVolume(bus, 0.8);
```

### Filter Parameters (Wet/Dry Mix)

The convolution filter has two parameters:
- **Wet** (index 0): The volume of the reverb effect (0.0 - 1.0, default 1.0)
- **Dry** (index 1): The volume of the original dry signal (0.0 - 1.0, default 0.5)

To adjust these parameters, use the filter API with a bus handle:

```dart
// Set wet/dry mix for convolution filter on a bus
// Using the filters API from SoLoud.instance.filters
// The bus handle can be used similarly to a SoundHandle for filter params
```

## Complete Example: Three Reverb Busses

The following example demonstrates how to set up three separate reverb busses with different impulse responses:

```dart
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_soloud/flutter_soloud.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await SoLoud.instance.init();
  runApp(const MyApp());
}

class MyApp extends StatefulWidget {
  const MyApp({super.key});

  @override
  State<MyApp> createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  // Audio sources
  AudioSource? _voiceSource;
  AudioSource? _musicSource;
  AudioSource? _sfxSource;

  // Bus handles - three separate reverb busses
  BusHandle? _roomReverbBus;
  BusHandle? _hallReverbBus;
  BusHandle? _cathedralReverbBus;

  // Track playing handles
  final List<SoundHandle> _playingHandles = [];

  // Available IR files (configure these paths to your assets)
  final List<String> _irFiles = [
    'assets/audio/IR/Alexandrinsky_MBg1v2.wav',   // Room-like
    'assets/audio/IR/BlackPool Tower_MQg1v2.wav', // Hall-like
    'assets/audio/IR/The Green Room_scg1v2.wav',  // Cathedral-like
  ];

  @override
  void initState() {
    super.initState();
    _initializeAudio();
  }

  Future<void> _initializeAudio() async {
    // Load audio sources
    _voiceSource = await SoLoud.instance.loadAsset('assets/audio/speech.mp3');
    _musicSource = await SoLoud.instance.loadAsset('assets/audio/music.mp3');
    _sfxSource = await SoLoud.instance.loadAsset('assets/audio/sfx.wav');

    // Create three reverb busses
    _roomReverbBus = SoLoud.instance.createBus();
    _hallReverbBus = SoLoud.instance.createBus();
    _cathedralReverbBus = SoLoud.instance.createBus();

    // Setup Room reverb bus
    SoLoud.instance.addBusFilter(_roomReverbBus!, FilterType.convolutionFilter);
    await _loadIrToBus(_roomReverbBus!, _irFiles[0]);

    // Setup Hall reverb bus
    SoLoud.instance.addBusFilter(_hallReverbBus!, FilterType.convolutionFilter);
    await _loadIrToBus(_hallReverbBus!, _irFiles[1]);

    // Setup Cathedral reverb bus
    SoLoud.instance.addBusFilter(_cathedralReverbBus!, FilterType.convolutionFilter);
    await _loadIrToBus(_cathedralReverbBus!, _irFiles[2]);

    setState(() {});
  }

  Future<void> _loadIrToBus(BusHandle bus, String irPath) async {
    // Load the IR file as an audio source temporarily
    final irSource = await SoLoud.instance.loadAsset(irPath);
    if (irSource.path != null) {
      SoLoud.instance.loadBusConvolutionIR(bus, irSource.path!);
    }
    // Dispose the IR source after loading (we only needed the path)
    await SoLoud.instance.disposeSource(irSource);
  }

  void _playOnBus(BusHandle? bus, AudioSource source) async {
    if (bus == null || source == null) return;

    final handle = await SoLoud.instance.playOnBus(bus, source, looping: true);
    _playingHandles.add(handle);
    setState(() {});
  }

  void _stopAllSounds() async {
    for (final handle in _playingHandles) {
      await SoLoud.instance.stop(handle);
    }
    _playingHandles.clear();
    setState(() {});
  }

  @override
  void dispose() {
    _stopAllSounds();

    // Clean up audio sources
    if (_voiceSource != null) SoLoud.instance.disposeSource(_voiceSource!);
    if (_musicSource != null) SoLoud.instance.disposeSource(_musicSource!);
    if (_sfxSource != null) SoLoud.instance.disposeSource(_sfxSource!);

    // Destroy busses
    if (_roomReverbBus != null) SoLoud.instance.destroyBus(_roomReverbBus!);
    if (_hallReverbBus != null) SoLoud.instance.destroyBus(_hallReverbBus!);
    if (_cathedralReverbBus != null) SoLoud.instance.destroyBus(_cathedralReverbBus!);

    SoLoud.instance.deinit();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        appBar: AppBar(title: const Text('Three Reverb Busses Demo')),
        body: Center(
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              // Voice playback controls
              const Text('Voice'),
              Row(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  ElevatedButton(
                    onPressed: () => _playOnBus(_roomReverbBus!, _voiceSource),
                    child: const Text('Room'),
                  ),
                  ElevatedButton(
                    onPressed: () => _playOnBus(_hallReverbBus!, _voiceSource),
                    child: const Text('Hall'),
                  ),
                  ElevatedButton(
                    onPressed: () => _playOnBus(_cathedralReverbBus!, _voiceSource),
                    child: const Text('Cathedral'),
                  ),
                ],
              ),
              const SizedBox(height: 20),

              // Music playback controls
              const Text('Music'),
              Row(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  ElevatedButton(
                    onPressed: () => _playOnBus(_roomReverbBus!, _musicSource),
                    child: const Text('Room'),
                  ),
                  ElevatedButton(
                    onPressed: () => _playOnBus(_hallReverbBus!, _musicSource),
                    child: const Text('Hall'),
                  ),
                  ElevatedButton(
                    onPressed: () => _playOnBus(_cathedralReverbBus!, _musicSource),
                    child: const Text('Cathedral'),
                  ),
                ],
              ),
              const SizedBox(height: 20),

              // SFX playback controls
              const Text('SFX'),
              Row(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  ElevatedButton(
                    onPressed: () => _playOnBus(_roomReverbBus!, _sfxSource),
                    child: const Text('Room'),
                  ),
                  ElevatedButton(
                    onPressed: () => _playOnBus(_hallReverbBus!, _sfxSource),
                    child: const Text('Hall'),
                  ),
                  ElevatedButton(
                    onPressed: () => _playOnBus(_cathedralReverbBus!, _sfxSource),
                    child: const Text('Cathedral'),
                  ),
                ],
              ),
              const SizedBox(height: 40),

              ElevatedButton(
                onPressed: _stopAllSounds,
                style: ElevatedButton.styleFrom(backgroundColor: Colors.red),
                child: const Text('Stop All'),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
```

## Practical Usage Patterns

### 1. Voice Chat with Reverb

For a voice chat application, you might want to apply different reverbs based on the room type the user selects:

```dart
// Create a bus for each room type
final smallRoomBus = SoLoud.instance.createBus();
final largeHallBus = SoLoud.instance.createBus();

// Add convolution filters
SoLoud.instance.addBusFilter(smallRoomBus, FilterType.convolutionFilter);
SoLoud.instance.addBusFilter(largeHallBus, FilterType.convolutionFilter);

// Load IRs
SoLoud.instance.loadBusConvolutionIR(smallRoomBus, 'assets/IR/small_room.wav');
SoLoud.instance.loadBusConvolutionIR(largeHallBus, 'assets/IR/concert_hall.wav');

// Play voice on selected bus
await SoLoud.instance.playOnBus(selectedBus, voiceSource);
```

### 2. Background Music with Environmental Reverb

Apply reverb to background music to match the visual environment:

```dart
// Forest environment
final forestBus = SoLoud.instance.createBus();
SoLoud.instance.addBusFilter(forestBus, FilterType.convolutionFilter);
SoLoud.instance.loadBusConvolutionIR(forestBus, 'assets/IR/forest_outdoor.wav');

// Cave environment  
final caveBus = SoLoud.instance.createBus();
SoLoud.instance.addBusFilter(caveBus, FilterType.convolutionFilter);
SoLoud.instance.loadBusConvolutionIR(caveBus, 'assets/IR/cave_large.wav');

// Play music on environment-matched bus
await SoLoud.instance.playOnBus(forestBus, musicSource);
```

### 3. Multiple Sound Categories with Individual Reverb

Apply different reverbs to different sound categories:

```dart
// Voiceover bus - subtle room reverb
final voiceoverBus = SoLoud.instance.createBus();
SoLoud.instance.addBusFilter(voiceoverBus, FilterType.convolutionFilter);
SoLoud.instance.loadBusConvolutionIR(voiceoverBus, 'assets/IR/studio_room.wav');

// Ambient sounds bus - large space
final ambientBus = SoLoud.instance.createBus();
SoLoud.instance.addBusFilter(ambientBus, FilterType.convolutionFilter);
SoLoud.instance.loadBusConvolutionIR(ambientBus, 'assets/IR/huge_hall.wav');

// UI sounds bus - short room
final uiBus = SoLoud.instance.createBus();
SoLoud.instance.addBusFilter(uiBus, FilterType.convolutionFilter);
SoLoud.instance.loadBusConvolutionIR(uiBus, 'assets/IR/small_ui.wav');
```

## IR File Sources

Free impulse response files can be found at:
- [OpenAir](https://openairlib.net/) - University of York acoustic database
- [IRs](https://www.irsmi.com/) - Commercial and free IRs
- Various audio forums and communities

## Notes

- IR files are loaded into memory when calling `loadBusConvolutionIR()`. Large IR files will consume more memory.
- You can change the IR on a bus at runtime by calling `loadBusConvolutionIR()` again with a new path.
- The convolution filter can be combined with other bus filters (e.g., EQ, compressor) for more complex processing chains.
- Bus volume can be faded using `fadeFilterParameter()` if needed for smooth transitions.
