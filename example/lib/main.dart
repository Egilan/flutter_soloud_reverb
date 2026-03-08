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
  String irA = '';
  String irB = '';

  double volA = 1.0;
  double volB = 1.0;
  
  bool isBusAActive = false;
  bool isBusBActive = false;

  @override
  void initState() {
    super.initState();
    _initDemo();
  }

  Future<void> _initDemo() async {
    // 1. Discover IRs
    final AssetManifest manifest = await AssetManifest.loadFromAssetBundle(rootBundle);
    final assets = manifest
        .listAssets()
        .where((String path) => path.startsWith('assets/audio/IR/') && path.endsWith('.wav'))
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
    speech = await SoLoud.instance.loadAsset('assets/audio/speech_20250803074809927_01.mp3');
    music = await SoLoud.instance.loadAsset('assets/audio/8_bit_mentality.mp3');
    
    // 3. Create buses
    busA = SoLoud.instance.createBus();
    busB = SoLoud.instance.createBus();

    // 4. Setup initial IRs on buses
    if (irA.isNotEmpty) {
      SoLoud.instance.addBusFilter(busA!, FilterType.convolutionFilter);
      await _loadIrToBus(busA!, irA);
    }
    if (irB.isNotEmpty) {
      SoLoud.instance.addBusFilter(busB!, FilterType.convolutionFilter);
      await _loadIrToBus(busB!, irB);
    }

    setState(() {
      isBusAActive = true;
      isBusBActive = true;
    });
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
      appBar: AppBar(title: const Text("SoLoud Multi-Bus Demo")),
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            _buildBusControl("Bus A", busA, irA, volA, (val) {
              setState(() => volA = val);
              SoLoud.instance.setBusVolume(busA!, val);
            }, (asset) async {
              setState(() => irA = asset);
              await _loadIrToBus(busA!, asset);
            }),
            const SizedBox(height: 24),
            _buildBusControl("Bus B", busB, irB, volB, (val) {
              setState(() => volB = val);
              SoLoud.instance.setBusVolume(busB!, val);
            }, (asset) async {
              setState(() => irB = asset);
              await _loadIrToBus(busB!, asset);
            }),
            const Divider(height: 48),
            _buildSoundControl("Speech", speech, () => _playSpeech(null), () => _playSpeech(busA), () => _playSpeech(busB)),
            const SizedBox(height: 24),
            _buildSoundControl("Music", music, () => _playMusic(null), () => _playMusic(busA), () => _playMusic(busB)),
          ],
        ),
      ),
    );
  }

  Widget _buildBusControl(String name, BusHandle? handle, String currentIr, double vol, ValueChanged<double> onVolChanged, ValueChanged<String> onIrChanged) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(name, style: const TextStyle(fontSize: 20, fontWeight: FontWeight.bold)),
            const SizedBox(height: 8),
            Row(
              children: [
                const Text("Volume"),
                Expanded(
                  child: Slider(value: vol, onChanged: onVolChanged),
                ),
                Text(vol.toStringAsFixed(2)),
              ],
            ),
            Row(
              children: [
                const Text("IR"),
                const SizedBox(width: 16),
                Expanded(
                  child: DropdownButton<String>(
                    value: currentIr,
                    isExpanded: true,
                    items: _irAssets.map((asset) => DropdownMenuItem(value: asset, child: Text(asset.split('/').last))).toList(),
                    onChanged: (val) => val != null ? onIrChanged(val) : null,
                  ),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildSoundControl(String name, AudioSource? source, VoidCallback onMain, VoidCallback onBusA, VoidCallback onBusB) {
    final isPlaying = source != null && source.handles.isNotEmpty;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(name, style: const TextStyle(fontSize: 18, fontWeight: FontWeight.w500)),
        const SizedBox(height: 8),
        Wrap(
          spacing: 8,
          children: [
            ElevatedButton(onPressed: onMain, child: const Text("Play Main")),
            ElevatedButton(onPressed: onBusA, child: const Text("Play on Bus A")),
            ElevatedButton(onPressed: onBusB, child: const Text("Play on Bus B")),
            if (isPlaying)
              IconButton(onPressed: () {
                for (final h in source.handles.toList()) {
                  SoLoud.instance.stop(h);
                }
                setState(() {});
              }, icon: const Icon(Icons.stop, color: Colors.red)),
          ],
        ),
      ],
    );
  }
}
