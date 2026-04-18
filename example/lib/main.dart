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

// ─── Dattorro parameter bundle ────────────────────────────────────────────────

class DattorroParams {
  final double preDelay;
  final double decay;
  final double damping;
  final double wet;
  final double dry;
  final double bandwidth;
  final double inputDiffusion;
  final double lfoRate;
  final double lfoDepth;

  const DattorroParams({
    required this.preDelay,
    required this.decay,
    required this.damping,
    required this.wet,
    required this.dry,
    required this.bandwidth,
    required this.inputDiffusion,
    required this.lfoRate,
    required this.lfoDepth,
  });

  DattorroParams copyWith({
    double? preDelay,
    double? decay,
    double? damping,
    double? wet,
    double? dry,
    double? bandwidth,
    double? inputDiffusion,
    double? lfoRate,
    double? lfoDepth,
  }) =>
      DattorroParams(
        preDelay: preDelay ?? this.preDelay,
        decay: decay ?? this.decay,
        damping: damping ?? this.damping,
        wet: wet ?? this.wet,
        dry: dry ?? this.dry,
        bandwidth: bandwidth ?? this.bandwidth,
        inputDiffusion: inputDiffusion ?? this.inputDiffusion,
        lfoRate: lfoRate ?? this.lfoRate,
        lfoDepth: lfoDepth ?? this.lfoDepth,
      );
}

// ─── Presets ──────────────────────────────────────────────────────────────────

class DattorroPreset {
  final String name;
  final DattorroParams params;
  const DattorroPreset(this.name, this.params);
}

const _presets = [
  DattorroPreset(
    'Default',
    DattorroParams(
      preDelay: 0.0,
      decay: 0.70,
      damping: 0.50,
      wet: 0.80,
      dry: 0.50,
      bandwidth: 1.0,
      inputDiffusion: 1.0,
      lfoRate: 1.0,
      lfoDepth: 1.0,
    ),
  ),
  DattorroPreset(
    'Floating Bird',
    // Long ambient Lexicon-style: maximum diffusion, near-infinite decay,
    // warm damping, slow LFO for that swirling chorused ghostliness.
    DattorroParams(
      preDelay: 0.0,
      decay: 0.97,
      damping: 0.65,
      wet: 0.85,
      dry: 0.30,
      bandwidth: 0.45,
      inputDiffusion: 1.0,
      lfoRate: 0.5,
      lfoDepth: 1.0,
    ),
  ),
  DattorroPreset(
    'Large Hall',
    DattorroParams(
      preDelay: 0.08,
      decay: 0.88,
      damping: 0.35,
      wet: 0.75,
      dry: 0.45,
      bandwidth: 0.75,
      inputDiffusion: 0.9,
      lfoRate: 0.8,
      lfoDepth: 0.8,
    ),
  ),
  DattorroPreset(
    'Small Room',
    DattorroParams(
      preDelay: 0.0,
      decay: 0.30,
      damping: 0.70,
      wet: 0.50,
      dry: 0.80,
      bandwidth: 0.9,
      inputDiffusion: 0.8,
      lfoRate: 1.5,
      lfoDepth: 0.4,
    ),
  ),
  DattorroPreset(
    'Dark Cave',
    DattorroParams(
      preDelay: 0.12,
      decay: 0.92,
      damping: 0.85,
      wet: 0.90,
      dry: 0.20,
      bandwidth: 0.2,
      inputDiffusion: 1.0,
      lfoRate: 0.3,
      lfoDepth: 0.6,
    ),
  ),
  DattorroPreset(
    'Metal Plate',
    DattorroParams(
      preDelay: 0.0,
      decay: 0.60,
      damping: 0.15,
      wet: 0.70,
      dry: 0.60,
      bandwidth: 1.0,
      inputDiffusion: 0.7,
      lfoRate: 2.5,
      lfoDepth: 0.5,
    ),
  ),
  DattorroPreset(
    'Infinite Wash',
    DattorroParams(
      preDelay: 0.15,
      decay: 0.99,
      damping: 0.60,
      wet: 1.0,
      dry: 0.0,
      bandwidth: 0.35,
      inputDiffusion: 1.0,
      lfoRate: 0.4,
      lfoDepth: 1.0,
    ),
  ),
];

// ─── App ──────────────────────────────────────────────────────────────────────

enum ReverbMode { none, ir, dattorro, hybrid }

class MultiBusDemo extends StatefulWidget {
  const MultiBusDemo({super.key});

  @override
  State<MultiBusDemo> createState() => _MultiBusDemoState();
}

class _MultiBusDemoState extends State<MultiBusDemo> {
  AudioSource? speech;
  AudioSource? music;

  BusHandle? busA;
  BusHandle? busB;

  List<String> _irAssets = [];

  ReverbMode modeA = ReverbMode.ir;
  ReverbMode modeB = ReverbMode.ir;

  String irA = '';
  String irB = '';

  double volA = 1.0;
  double volB = 1.0;

  DattorroParams paramsA = _presets.first.params;
  DattorroParams paramsB = _presets.first.params;

  bool isReady = false;

  @override
  void initState() {
    super.initState();
    _initDemo();
  }

  Future<void> _initDemo() async {
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

    speech = await SoLoud.instance
        .loadAsset('assets/audio/speech_20250803074809927_01.mp3');
    music = await SoLoud.instance.loadAsset('assets/audio/8_bit_mentality.mp3');

    busA = SoLoud.instance.createBus();
    busB = SoLoud.instance.createBus();

    await _applyMode(busA!, modeA, isA: true);
    await _applyMode(busB!, modeB, isA: false);

    setState(() => isReady = true);
  }

  Future<void> _applyMode(BusHandle bus, ReverbMode mode,
      {required bool isA}) async {
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
      case ReverbMode.dattorro:
        SoLoud.instance.addBusFilter(bus, FilterType.dattorroFilter);
        _pushDattorroParams(bus, isA ? paramsA : paramsB);
      case ReverbMode.hybrid:
        if (ir.isNotEmpty) {
          SoLoud.instance.addBusFilter(bus, FilterType.convolutionFilter);
          await _loadIrToBus(bus, ir);
        }
        SoLoud.instance.addBusFilter(bus, FilterType.dattorroFilter);
        _pushDattorroParams(bus, isA ? paramsA : paramsB);
    }
  }

  void _pushDattorroParams(BusHandle bus, DattorroParams p) {
    void set(int idx, double val) =>
        SoLoud.instance.setBusFilterParameter(
            bus, FilterType.dattorroFilter, idx, val);
    set(0, p.preDelay);
    set(1, p.decay);
    set(2, p.damping);
    set(3, p.wet);
    set(4, p.dry);
    set(5, p.bandwidth);
    set(6, p.inputDiffusion);
    set(7, p.lfoRate);
    set(8, p.lfoDepth);
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
      appBar: AppBar(title: const Text('SoLoud Multi-Bus Reverb Demo')),
      body: !isReady
          ? const Center(child: CircularProgressIndicator())
          : SingleChildScrollView(
              padding: const EdgeInsets.all(16),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  _buildBusControl('Bus A', busA!, isA: true),
                  const SizedBox(height: 24),
                  _buildBusControl('Bus B', busB!, isA: false),
                  const Divider(height: 48),
                  _buildSoundControl(
                    'Speech',
                    speech,
                    () => _playSpeech(null),
                    () => _playSpeech(busA),
                    () => _playSpeech(busB),
                  ),
                  const SizedBox(height: 24),
                  _buildSoundControl(
                    'Music',
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

  Widget _buildBusControl(String name, BusHandle handle, {required bool isA}) {
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
                style: const TextStyle(
                    fontSize: 20, fontWeight: FontWeight.bold)),
            const SizedBox(height: 8),

            // Volume
            _slider(
              'Volume',
              vol,
              0.0,
              1.0,
              (val) {
                setState(() => isA ? volA = val : volB = val);
                SoLoud.instance.setBusVolume(handle, val);
              },
            ),

            // Mode selector
            Row(
              children: [
                const Text('Mode'),
                const SizedBox(width: 16),
                Expanded(
                  child: SegmentedButton<ReverbMode>(
                    segments: const [
                      ButtonSegment(
                          value: ReverbMode.none, label: Text('None')),
                      ButtonSegment(
                          value: ReverbMode.ir, label: Text('IR')),
                      ButtonSegment(
                          value: ReverbMode.dattorro,
                          label: Text('Dattorro')),
                      ButtonSegment(
                          value: ReverbMode.hybrid, label: Text('Hybrid')),
                    ],
                    selected: {mode},
                    onSelectionChanged: (selected) async {
                      final newMode = selected.first;
                      setState(() =>
                          isA ? modeA = newMode : modeB = newMode);
                      await _applyMode(handle, newMode, isA: isA);
                    },
                  ),
                ),
              ],
            ),
            const SizedBox(height: 8),

            // IR selector
            if (mode == ReverbMode.ir || mode == ReverbMode.hybrid)
              Row(
                children: [
                  const Text('IR'),
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
                        setState(
                            () => isA ? irA = val : irB = val);
                        await _loadIrToBus(handle, val);
                      },
                    ),
                  ),
                ],
              ),

            // Dattorro controls
            if (mode == ReverbMode.dattorro || mode == ReverbMode.hybrid)
              _buildDattorroControls(handle, isA: isA),
          ],
        ),
      ),
    );
  }

  Widget _buildDattorroControls(BusHandle bus, {required bool isA}) {
    final p = isA ? paramsA : paramsB;

    void update(DattorroParams next) {
      setState(() => isA ? paramsA = next : paramsB = next);
      _pushDattorroParams(bus, next);
    }

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        const SizedBox(height: 8),

        // ── Preset picker ─────────────────────────────────────────────────
        Row(
          children: [
            const Text('Preset',
                style: TextStyle(fontWeight: FontWeight.w600)),
            const SizedBox(width: 16),
            Expanded(
              child: DropdownButton<DattorroPreset>(
                isExpanded: true,
                hint: const Text('Select preset…'),
                value: null,
                items: _presets
                    .map((preset) => DropdownMenuItem(
                          value: preset,
                          child: Text(preset.name),
                        ))
                    .toList(),
                onChanged: (preset) {
                  if (preset == null) return;
                  update(preset.params);
                },
              ),
            ),
          ],
        ),

        const Divider(),

        // ── Signal path ───────────────────────────────────────────────────
        _sectionLabel('Signal Path'),
        _slider('Pre-Delay', p.preDelay, 0.0, 1.0,
            (v) => update(p.copyWith(preDelay: v))),
        _slider('Bandwidth', p.bandwidth, 0.0, 1.0,
            (v) => update(p.copyWith(bandwidth: v)),
            hint: 'Input low-pass before diffusion (0=dark, 1=bright)'),
        _slider('Input Diffusion', p.inputDiffusion, 0.0, 1.0,
            (v) => update(p.copyWith(inputDiffusion: v)),
            hint: 'Smearing of the input signal (0=echoes, 1=cloud)'),

        // ── Tank ──────────────────────────────────────────────────────────
        _sectionLabel('Tank'),
        _slider('Decay', p.decay, 0.0, 0.99,
            (v) => update(p.copyWith(decay: v)),
            hint: 'Feedback amount — push toward 0.99 for infinite tail'),
        _slider('Damping', p.damping, 0.0, 0.99,
            (v) => update(p.copyWith(damping: v)),
            hint: 'HF roll-off in tank (higher = warmer, darker tail)'),

        // ── Modulation ────────────────────────────────────────────────────
        _sectionLabel('Modulation (LFO)'),
        _slider('LFO Rate (Hz)', p.lfoRate, 0.1, 10.0,
            (v) => update(p.copyWith(lfoRate: v)),
            hint: '0.5–1 Hz for lush chorus; faster = vibrato effect'),
        _slider('LFO Depth', p.lfoDepth, 0.0, 1.0,
            (v) => update(p.copyWith(lfoDepth: v)),
            hint: 'Modulation depth — the "Lexicon secret sauce"'),

        // ── Mix ───────────────────────────────────────────────────────────
        _sectionLabel('Mix'),
        _slider('Wet', p.wet, 0.0, 1.0,
            (v) => update(p.copyWith(wet: v))),
        _slider('Dry', p.dry, 0.0, 1.0,
            (v) => update(p.copyWith(dry: v))),
      ],
    );
  }

  Widget _sectionLabel(String label) => Padding(
        padding: const EdgeInsets.only(top: 8, bottom: 2),
        child: Text(label,
            style: const TextStyle(
                fontSize: 12,
                fontWeight: FontWeight.w600,
                letterSpacing: 0.8,
                color: Colors.white54)),
      );

  Widget _slider(
    String label,
    double value,
    double min,
    double max,
    ValueChanged<double> onChanged, {
    String? hint,
  }) {
    return Tooltip(
      message: hint ?? '',
      child: Row(
        children: [
          SizedBox(width: 110, child: Text(label, style: const TextStyle(fontSize: 13))),
          Expanded(
            child: Slider(value: value, min: min, max: max, onChanged: onChanged),
          ),
          SizedBox(
              width: 44,
              child: Text(value.toStringAsFixed(2),
                  style: const TextStyle(fontSize: 12))),
        ],
      ),
    );
  }

  Widget _buildSoundControl(
    String name,
    AudioSource? source,
    VoidCallback onMain,
    VoidCallback onBusA,
    VoidCallback onBusB,
  ) {
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
            ElevatedButton(onPressed: onMain, child: const Text('Play Main')),
            ElevatedButton(
                onPressed: onBusA, child: const Text('Play on Bus A')),
            ElevatedButton(
                onPressed: onBusB, child: const Text('Play on Bus B')),
            if (isPlaying)
              IconButton(
                onPressed: () {
                  for (final h in source.handles.toList()) {
                    SoLoud.instance.stop(h);
                  }
                  setState(() {});
                },
                icon: const Icon(Icons.stop, color: Colors.red),
              ),
          ],
        ),
      ],
    );
  }
}
