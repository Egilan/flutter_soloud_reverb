#
# To learn more about a Podspec see http://guides.cocoapods.org/syntax/podspec.html.
# Run `pod lib lint flutter_soloud.podspec` to validate before publishing.
#
Pod::Spec.new do |s|
  s.name             = 'flutter_soloud'
  s.version          = '0.0.1'
  s.summary          = 'Flutter audio plugin using SoLoud library and FFI'
  s.description      = <<-DESC
Flutter audio plugin using SoLoud library and FFI
                       DESC
  s.homepage         = 'http://example.com'
  s.license          = { :file => '../LICENSE' }
  s.author           = { 'Your Company' => 'email@example.com' }

  # This will ensure the source files in Classes/ are included in the native
  # builds of apps using this FFI plugin. Podspec does not support relative
  # paths, so Classes contains a forwarder C file that relatively imports
  # `../src/*` so that the C sources can be shared among all target platforms.
  s.source           = { :path => '.' }
  s.source_files = 'Classes/**/*',
                     '../third_party/libpd/pure-data/src/**/*.{h,c}',
                     '../third_party/libpd/pure-data/extra/bob~/*.{h,c}',
                     '../third_party/libpd/pure-data/extra/bonk~/*.{h,c}',
                     '../third_party/libpd/pure-data/extra/choice/*.{h,c}',
                     '../third_party/libpd/pure-data/extra/fiddle~/*.{h,c}',
                     '../third_party/libpd/pure-data/extra/loop~/*.{h,c}',
                     '../third_party/libpd/pure-data/extra/lrshift~/*.{h,c}',
                     '../third_party/libpd/pure-data/extra/pd~/pdsched.c',
                     '../third_party/libpd/pure-data/extra/pd~/pd~.c',
                     '../third_party/libpd/pure-data/extra/pique/*.{h,c}',
                     '../third_party/libpd/pure-data/extra/sigmund~/*.{h,c}',
                     '../third_party/libpd/pure-data/extra/stdout/*.{h,c}',
                     '../third_party/libpd/libpd_wrapper/**/*.{h,c}'
  s.dependency 'Flutter'
  s.platform = :ios, '13.0'

  # Check if we should disable opus/ogg support (must exist and be '1')
  disable_opus_ogg = !ENV['NO_OPUS_OGG_LIBS'].nil? && ENV['NO_OPUS_OGG_LIBS'] == '1'

  preprocessor_definitions = ['$(inherited)']
  if disable_opus_ogg
    preprocessor_definitions << 'NO_OPUS_OGG_LIBS'
  end

  # Exclude libpd files not needed (backends, duplicate sources)
  s.exclude_files = '../third_party/libpd/pure-data/src/s_audio_alsa.{h,c}',
                    '../third_party/libpd/pure-data/src/s_audio_alsamm.c',
                    '../third_party/libpd/pure-data/src/s_audio_audiounit.c',
                    '../third_party/libpd/pure-data/src/s_audio_esd.c',
                    '../third_party/libpd/pure-data/src/s_audio_jack.c',
                    '../third_party/libpd/pure-data/src/s_audio_mmio.c',
                    '../third_party/libpd/pure-data/src/s_audio_oss.c',
                    '../third_party/libpd/pure-data/src/s_audio_pa.c',
                    '../third_party/libpd/pure-data/src/s_audio_paring.{h,c}',
                    '../third_party/libpd/pure-data/src/s_file.c',
                    '../third_party/libpd/pure-data/src/s_midi_alsa.c',
                    '../third_party/libpd/pure-data/src/s_midi_dummy.c',
                    '../third_party/libpd/pure-data/src/s_midi_mmio.c',
                    '../third_party/libpd/pure-data/src/s_midi_oss.c',
                    '../third_party/libpd/pure-data/src/s_midi_pm.c',
                    '../third_party/libpd/pure-data/src/s_midi.c',
                    '../third_party/libpd/pure-data/src/d_fft_fftw.c',
                    '../third_party/libpd/pure-data/src/s_entry.c',
                    '../third_party/libpd/pure-data/src/s_watchdog.c',
                    '../third_party/libpd/pure-data/src/u_pdreceive.c',
                    '../third_party/libpd/pure-data/src/u_pdsend.c',
                    '../third_party/libpd/pure-data/src/x_libpdreceive.{h,c}',
                    '../third_party/libpd/pure-data/src/z_ringbuffer.{h,c}',
                    '../third_party/libpd/pure-data/src/z_queued.{h,c}',
                    '../third_party/libpd/pure-data/src/z_print_util.{h,c}',
                    '../third_party/libpd/pure-data/src/z_hooks.{h,c}',
                    '../third_party/libpd/pure-data/src/s_libpdmidi.c',
                    '../third_party/libpd/pure-data/src/z_libpd.{h,c}',
                    '../third_party/libpd/pure-data/src/m_dispatch_gen.c',
                    '../third_party/libpd/objc/**/*'

  s.compiler_flags = [
    '-w',
    '-DOS_OBJECT_USE_OBJC=0',
    '-Wno-format',
    '-lpthread',
    '-lm',
    '-DPD', '-DUSEAPI_DUMMY', '-DPD_INTERNAL',
    '-DHAVE_UNISTD_H', '-DHAVE_ALLOCA_H',
    '-DHAVE_MACHINE_ENDIAN_H', '-D_DARWIN_C_SOURCE',
    '-D_DARWIN_UNLIMITED_SELECT', '-DFD_SETSIZE=10240',
    '-DLIBPD_EXTRA', '-fcommon'
  ]

  # Flutter.framework does not contain a i386 slice.
  s.pod_target_xcconfig = { 
    'HEADER_SEARCH_PATHS' => [
      '$(PODS_TARGET_SRCROOT)/include',
      '$(PODS_TARGET_SRCROOT)/include/opus',
      '$(PODS_TARGET_SRCROOT)/include/ogg',
      '$(PODS_TARGET_SRCROOT)/include/vorbis',
      '$(PODS_TARGET_SRCROOT)/../src',
      '$(PODS_TARGET_SRCROOT)/../src/soloud/include',
      '${PODS_ROOT}/abseil',
      '$(PODS_TARGET_SRCROOT)/../third_party/libpd/libpd_wrapper',
      '$(PODS_TARGET_SRCROOT)/../third_party/libpd/pure-data/src',
    ],
    'GCC_PREPROCESSOR_DEFINITIONS' => preprocessor_definitions.join(' '),
    'DEFINES_MODULE' => 'YES', 
    'VALID_ARCHS' => 'arm64 x86_64',
    'LIBRARY_SEARCH_PATHS' => [
      '$(PODS_TARGET_SRCROOT)/libs',
      '$(SRCROOT)/libs'
    ],
    'OTHER_LDFLAGS[sdk=iphonesimulator*]' => disable_opus_ogg ? '' : '-logg_iOS-simulator -lopus_iOS-simulator -lvorbis_iOS-simulator -lvorbisfile_iOS-simulator -lflac_iOS-simulator',
    'OTHER_LDFLAGS[sdk=iphoneos*]' => disable_opus_ogg ? '' : '-logg_iOS-device -lopus_iOS-device -lvorbis_iOS-device -lvorbisfile_iOS-device -lflac_iOS-device',
    "CLANG_CXX_LANGUAGE_STANDARD" => "c++17",
    "CLANG_CXX_LIBRARY" => "libc++"
  }
  
  # Only include libraries if opus/ogg is enabled
  if !disable_opus_ogg
    s.ios.vendored_libraries = [
      'libs/libopus_iOS-device.a',
      'libs/libogg_iOS-device.a',
      'libs/libvorbis_iOS-device.a',
      'libs/libvorbisfile_iOS-device.a',
      'libs/libflac_iOS-device.a'
    ]
    s.preserve_paths = [
      'libs/libopus_iOS-device.a',
      'libs/libogg_iOS-device.a',
      'libs/libopus_iOS-simulator.a',
      'libs/libogg_iOS-simulator.a',
      'libs/libvorbis_iOS-device.a',
      'libs/libvorbis_iOS-simulator.a',
      'libs/libvorbisfile_iOS-device.a',
      'libs/libvorbisfile_iOS-simulator.a',
      'libs/libflac_iOS-device.a',
      'libs/libflac_iOS-simulator.a',
      '../third_party/libpd/pure-data/extra/pd~/binarymsg.c'
    ]
  end

  s.swift_version = '5.0'
  s.ios.framework  = ['AudioToolbox', 'AVFAudio']
end
