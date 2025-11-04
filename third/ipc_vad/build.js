// Create voice activity detection library target
const vad = createTarget('ipc_vad');

// Get base directory

const baseDir = './';


const kind = config.getOrDefault('library_kind', "shared")
vad.setTargetType(kind);

// Add WebRTC VAD source files
vad.addFiles(baseDir + 'modules/audio_processing/vad/*.cc');
vad.addFiles(baseDir + 'modules/audio_coding/codecs/isac/main/source/isac_vad.c');
vad.addFiles(baseDir + 'modules/audio_coding/codecs/isac/main/source/pitch_estimator.c');
vad.addFiles(baseDir + 'modules/audio_coding/codecs/isac/main/source/pitch_filter.c');
vad.addFiles(baseDir + 'modules/audio_coding/codecs/isac/main/source/filter_functions.c');

// Add common audio source files
vad.addFiles(baseDir + 'common_audio/vad/*.c');
vad.addFiles(baseDir + 'common_audio/resampler/resampler.cc');

// Add signal processing source files
vad.addFiles(baseDir + 'common_audio/signal_processing/resample.c');
vad.addFiles(baseDir + 'common_audio/signal_processing/resample_by_2.c');
vad.addFiles(baseDir + 'common_audio/signal_processing/resample_by_2_internal.c');
vad.addFiles(baseDir + 'common_audio/signal_processing/resample_48khz.c');
vad.addFiles(baseDir + 'common_audio/signal_processing/resample_fractional.c');
vad.addFiles(baseDir + 'common_audio/signal_processing/division_operations.c');
vad.addFiles(baseDir + 'common_audio/signal_processing/energy.c');
vad.addFiles(baseDir + 'common_audio/signal_processing/get_scaling_square.c');
vad.addFiles(baseDir + 'common_audio/third_party/ooura/fft_size_256/fft4g.cc');

// Add IPC VAD and base library source files
vad.addFiles(baseDir + 'api/src/*.c');
vad.addFiles(baseDir + 'rtc_base/checks.cc');

// Add compilation flags
vad.addCflags('-DWEBRTC_POSIX', '-DRTC_DISABLE_CHECK_MSG');
vad.addCxxflags('-DWEBRTC_POSIX', '-DRTC_DISABLE_CHECK_MSG');

vad.removeCxxflags('-Werror');
vad.removeCflags('-Werror');

// Add include directories
vad.addIncludeDirs(baseDir + '.', baseDir + 'abseil-port');
vad.addIncludeDirs(baseDir + 'api/include', true);

// Build IPC VAD library
if (vad.build()) {
    console.log('IPC VAD library build successful');
} else {
    console.error('IPC VAD library build failed');
    process.exit(1);
}