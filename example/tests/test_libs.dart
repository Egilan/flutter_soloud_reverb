// ignore_for_file: cascade_invocations

import 'package:flutter/cupertino.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_soloud/flutter_soloud.dart';

void main() {
  runApp(
    CupertinoApp(
      home: Builder(
        builder: (testLibs) {
          readSamples();
          return CupertinoPageScaffold(
            child: FutureBuilder<bool>(
              future: readSamples(),
              builder: (context, asyncSnapshot) {
                return Container(
                  width: 300,
                  height: 300,
                  color: asyncSnapshot.hasData && (asyncSnapshot.data ?? false)
                      ? Colors.green
                      : Colors.red,
                );
              },
            ),
          );
        },
      ),
    ),
  );
}

Future<bool> readSamples() async {
  final soloud = SoLoud.instance;
  await soloud.init();
  final buffer = await rootBundle.load('assets/audio/sample-1.ogg');
  late Float32List samples;
  try {
    /// trhows if the libs are not linked
    samples =
        await soloud.readSamplesFromMem(buffer.buffer.asUint8List(), 2048);
  } catch (e) {
    debugPrint(e.toString());
    return false;
  }

  for (var i = 0; i < 10; i++) {
    debugPrint(samples[i].toString());
  }

  soloud.deinit();
  return true;
}
