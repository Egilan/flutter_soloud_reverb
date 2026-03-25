import 'dart:developer' as dev;

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_soloud/flutter_soloud.dart';
import 'package:logging/logging.dart';

void main() async {
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

  await SoLoud.instance.init();

  runApp(
    MaterialApp(
      theme: ThemeData.dark(useMaterial3: true),
      home: const MultiBusDemo(),
    ),
  );
}

enum ReverbMode { none, ir, dattorro, hybrid }

class MultiBusDemo extends StatefulWidget {
  const MultiBusDemo({super.key});

  @override
  State<MultiBusDemo> createState() => _MultiBusDemoState();
}

class _MultiBusDemoState extends State<MultiBusDemo> {
  AudioSource? speech;
  AudioSource? music;
  SoundHandle? speechHandle;
  SoundHandle? musicHandle;

  BusHandle? busA;
  BusHandle? busB;

  List<String> _irAssets = [];

  // Per-bus state
  ReverbMode modeA = ReverbMode.ir;
  ReverbMode modeB = ReverbMode.ir;

  String irA = '';
  String irB = '';

  double volA = 1.0;
  double volB = 1.0;

  // Dattorro params per bus
  double dattorroPreDelayA = 0.0;
  double dattorroDecayA = 0.7;
  double dattorroDampingA = 0.5;
  double dattorroWetA = 0.8;
  double dattorroDryA = 0.5;

  double dattorroPreDelayB = 0.0;
  double dattorroDecayB = 0.7;
  double dattorroDampingB = 0.5;
  double dattorroWetB = 0.8;
  double dattorroDryB = 0.5;

  bool isReady = false;

  @override
  void initState() {
    super.initState();
    _initDemo();
  }

  Future<void> _initDemo() async {
    // 1. Discover IRs
    final AssetManifest manifest =
        await AssetManifest.loadFromAssetBundle(rootBundle);
    final assets = manifest
        .listAssets()
        .where((String path) =>
            path.startsWith('assets/audio/IR/') && path.endsWith('.wav'))
        .toList();

    setState(() {
      _irAssets = assets;
      if (_irAssets.length >= 2) {
        irA = _irAssets[0];
        irB = _irAssets[1];
      } else if (_irAssets.isNotEmpty) {
        irA = irB = _irAssets.first;
      }
    });

    // 2. Load sounds
    speech = await SoLoud.instance
        .loadAsset('assets/audio/speech_20250803074809927_01.mp3');
    music = await SoLoud.instance.loadAsset('assets/audio/8_bit_mentality.mp3');

    // 3. Create buses
    busA = SoLoud.instance.createBus();
    busB = SoLoud.instance.createBus();

    // 4. Setup initial filters (IR mode by default)
    await _applyMode(busA!, modeA, isA: true);
    await _applyMode(busB!, modeB, isA: false);

    setState(() => isReady = true);
  }

  Future<void> _applyMode(BusHandle bus, ReverbMode mode,
      {required bool isA}) async {
    // Remove existing filters
    try {
      SoLoud.instance.removeBusFilter(bus, FilterType.convolutionFilter);
    } catch (_) {}
    try {
      SoLoud.instance.removeBusFilter(bus, FilterType.dattorroFilter);
    } catch (_) {}

    final ir = isA ? irA : irB;

    switch (mode) {
      case ReverbMode.none:
        break;
      case ReverbMode.ir:
        if (ir.isNotEmpty) {
          SoLoud.instance.addBusFilter(bus, FilterType.convolutionFilter);
          await _loadIrToBus(bus, ir);
        }
        break;
      case ReverbMode.dattorro:
        SoLoud.instance.addBusFilter(bus, FilterType.dattorroFilter);
        _applyDattorroParams(bus, isA: isA);
        break;
      case ReverbMode.hybrid:
        if (ir.isNotEmpty) {
          SoLoud.instance.addBusFilter(bus, FilterType.convolutionFilter);
          await _loadIrToBus(bus, ir);
        }
        SoLoud.instance.addBusFilter(bus, FilterType.dattorroFilter);
        _applyDattorroParams(bus, isA: isA);
        break;
    }
  }

  void _applyDattorroParams(BusHandle bus, {required bool isA}) {
    final preDelay = isA ? dattorroPreDelayA : dattorroPreDelayB;
    final decay = isA ? dattorroDecayA : dattorroDecayB;
    final damping = isA ? dattorroDampingA : dattorroDampingB;
    final wet = isA ? dattorroWetA : dattorroWetB;
    final dry = isA ? dattorroDryA : dattorroDryB;

    SoLoud.instance.setBusFilterParameter(
        bus, FilterType.dattorroFilter, 0, preDelay);
    SoLoud.instance
        .setBusFilterParameter(bus, FilterType.dattorroFilter, 1, decay);
    SoLoud.instance
        .setBusFilterParameter(bus, FilterType.dattorroFilter, 2, damping);
    SoLoud.instance
        .setBusFilterParameter(bus, FilterType.dattorroFilter, 3, wet);
    SoLoud.instance
        .setBusFilterParameter(bus, FilterType.dattorroFilter, 4, dry);
  }

  Future<void> _loadIrToBus(BusHandle bus, String assetPath) async {
    final irSource = await SoLoud.instance.loadAsset(assetPath);
    if (irSource.path != null) {
      SoLoud.instance.loadBusConvolutionIR(bus, irSource.path!);
    }
    await SoLoud.instance.disposeSource(irSource);
  }

  @override
  void dispose() {
    SoLoud.instance.deinit();
    super.dispose();
  }

  void _playSpeech(BusHandle? bus) async {
    if (bus == null) {
      await SoLoud.instance.play(speech!, looping: true);
    } else {
      await SoLoud.instance.playOnBus(bus, speech!, looping: true);
    }
    setState(() {});
  }

  void _playMusic(BusHandle? bus) async {
    if (bus == null) {
      await SoLoud.instance.play(music!, looping: true);
    } else {
      await SoLoud.instance.playOnBus(bus, music!, looping: true);
    }
    setState(() {});
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text("SoLoud Multi-Bus Reverb Demo")),
      body: !isReady
          ? const Center(child: CircularProgressIndicator())
          : SingleChildScrollView(
              padding: const EdgeInsets.all(16),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  _buildBusControl("Bus A", busA!, isA: true),
                  const SizedBox(height: 24),
                  _buildBusControl("Bus B", busB!, isA: false),
                  const Divider(height: 48),
                  _buildSoundControl(
                    "Speech",
                    speech,
                    () => _playSpeech(null),
                    () => _playSpeech(busA),
                    () => _playSpeech(busB),
                  ),
                  const SizedBox(height: 24),
                  _buildSoundControl(
                    "Music",
                    music,
                    () => _playMusic(null),
                    () => _playMusic(busA),
                    () => _playMusic(busB),
                  ),
                ],
              ),
            ),
    );
  }

  Widget _buildBusControl(String name, BusHandle handle,
      {required bool isA}) {
    final mode = isA ? modeA : modeB;
    final currentIr = isA ? irA : irB;
    final vol = isA ? volA : volB;

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(name,
                style:
                    const TextStyle(fontSize: 20, fontWeight: FontWeight.bold)),
            const SizedBox(height: 8),

            // Volume
            Row(
              children: [
                const Text("Volume"),
                Expanded(
                  child: Slider(
                    value: vol,
                    onChanged: (val) {
                      setState(() {
                        if (isA) {
                          volA = val;
                        } else {
                          volB = val;
                        }
                      });
                      SoLoud.instance.setBusVolume(handle, val);
                    },
                  ),
                ),
                Text(vol.toStringAsFixed(2)),
              ],
            ),

            // Mode selector
            Row(
              children: [
                const Text("Mode"),
                const SizedBox(width: 16),
                Expanded(
                  child: SegmentedButton<ReverbMode>(
                    segments: const [
                      ButtonSegment(
                          value: ReverbMode.none, label: Text("None")),
                      ButtonSegment(value: ReverbMode.ir, label: Text("IR")),
                      ButtonSegment(
                          value: ReverbMode.dattorro, label: Text("Dattorro")),
                      ButtonSegment(
                          value: ReverbMode.hybrid, label: Text("Hybrid")),
                    ],
                    selected: {mode},
                    onSelectionChanged: (selected) async {
                      final newMode = selected.first;
                      setState(() {
                        if (isA) {
                          modeA = newMode;
                        } else {
                          modeB = newMode;
                        }
                      });
                      await _applyMode(handle, newMode, isA: isA);
                    },
                  ),
                ),
              ],
            ),
            const SizedBox(height: 8),

            // IR selector (shown for IR and Hybrid modes)
            if (mode == ReverbMode.ir || mode == ReverbMode.hybrid)
              Row(
                children: [
                  const Text("IR"),
                  const SizedBox(width: 16),
                  Expanded(
                    child: DropdownButton<String>(
                      value: currentIr,
                      isExpanded: true,
                      items: _irAssets
                          .map((asset) => DropdownMenuItem(
                              value: asset,
                              child: Text(asset.split('/').last)))
                          .toList(),
                      onChanged: (val) async {
                        if (val == null) return;
                        setState(() {
                          if (isA) {
                            irA = val;
                          } else {
                            irB = val;
                          }
                        });
                        await _loadIrToBus(handle, val);
                      },
                    ),
                  ),
                ],
              ),

            // Dattorro params (shown for Dattorro and Hybrid modes)
            if (mode == ReverbMode.dattorro || mode == ReverbMode.hybrid)
              _buildDattorroControls(handle, isA: isA),
          ],
        ),
      ),
    );
  }

  Widget _buildDattorroControls(BusHandle bus, {required bool isA}) {
    return Column(
      children: [
        _dattorroSlider("Pre-Delay", isA ? dattorroPreDelayA : dattorroPreDelayB, 0.0, 1.0,
            (val) {
          setState(() {
            if (isA) {
              dattorroPreDelayA = val;
            } else {
              dattorroPreDelayB = val;
            }
          });
          SoLoud.instance
              .setBusFilterParameter(bus, FilterType.dattorroFilter, 0, val);
        }),
        _dattorroSlider(
            "Decay", isA ? dattorroDecayA : dattorroDecayB, 0.0, 0.99, (val) {
          setState(() {
            if (isA) {
              dattorroDecayA = val;
            } else {
              dattorroDecayB = val;
            }
          });
          SoLoud.instance
              .setBusFilterParameter(bus, FilterType.dattorroFilter, 1, val);
        }),
        _dattorroSlider("Damping", isA ? dattorroDampingA : dattorroDampingB,
            0.0, 0.99, (val) {
          setState(() {
            if (isA) {
              dattorroDampingA = val;
            } else {
              dattorroDampingB = val;
            }
          });
          SoLoud.instance
              .setBusFilterParameter(bus, FilterType.dattorroFilter, 2, val);
        }),
        _dattorroSlider("Wet", isA ? dattorroWetA : dattorroWetB, 0.0, 1.0,
            (val) {
          setState(() {
            if (isA) {
              dattorroWetA = val;
            } else {
              dattorroWetB = val;
            }
          });
          SoLoud.instance
              .setBusFilterParameter(bus, FilterType.dattorroFilter, 3, val);
        }),
        _dattorroSlider("Dry", isA ? dattorroDryA : dattorroDryB, 0.0, 1.0,
            (val) {
          setState(() {
            if (isA) {
              dattorroDryA = val;
            } else {
              dattorroDryB = val;
            }
          });
          SoLoud.instance
              .setBusFilterParameter(bus, FilterType.dattorroFilter, 4, val);
        }),
      ],
    );
  }

  Widget _dattorroSlider(String label, double value, double min, double max,
      ValueChanged<double> onChanged) {
    return Row(
      children: [
        SizedBox(width: 80, child: Text(label)),
        Expanded(
          child: Slider(value: value, min: min, max: max, onChanged: onChanged),
        ),
        SizedBox(width: 48, child: Text(value.toStringAsFixed(2))),
      ],
    );
  }

  Widget _buildSoundControl(String name, AudioSource? source,
      VoidCallback onMain, VoidCallback onBusA, VoidCallback onBusB) {
    final isPlaying = source != null && source.handles.isNotEmpty;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(name,
            style:
                const TextStyle(fontSize: 18, fontWeight: FontWeight.w500)),
        const SizedBox(height: 8),
        Wrap(
          spacing: 8,
          children: [
            ElevatedButton(
                onPressed: onMain, child: const Text("Play Main")),
            ElevatedButton(
                onPressed: onBusA, child: const Text("Play on Bus A")),
            ElevatedButton(
                onPressed: onBusB, child: const Text("Play on Bus B")),
            if (isPlaying)
              IconButton(
                  onPressed: () {
                    for (final h in source.handles.toList()) {
                      SoLoud.instance.stop(h);
                    }
                    setState(() {});
                  },
                  icon: const Icon(Icons.stop, color: Colors.red)),
          ],
        ),
      ],
    );
  }
}
