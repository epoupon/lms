pkg_check_modules(LIBAV IMPORTED_TARGET libavcodec libavutil libavformat libswresample)
pkg_check_modules(Taglib REQUIRED IMPORTED_TARGET taglib)
pkg_check_modules(PulseAudio IMPORTED_TARGET libpulse)
pkg_check_modules(ALSA IMPORTED_TARGET alsa)
pkg_check_modules(FFTW3f REQUIRED IMPORTED_TARGET fftw3f)

if (PulseAudio_FOUND OR ALSA_FOUND)
    message(STATUS "Audio output available (PulseAudio=${PulseAudio_FOUND}, ALSA=${ALSA_FOUND})")
else()
    message(STATUS "No audio output backend found")
endif()

add_library(lmsaudio STATIC
	impl/features/AudioFeaturesExtractor.cpp
	impl/features/DeltaCalculator.cpp
	impl/features/FFT.cpp
	impl/features/MelFilterBank.cpp
	impl/features/StatsAccumulator.cpp
	impl/features/Window.cpp
	impl/ffmpeg/AudioFile.cpp
	impl/ffmpeg/AudioFileInfo.cpp
	impl/ffmpeg/AudioFileInfoParser.cpp
	impl/ffmpeg/FFmpegTypes.cpp
	impl/ffmpeg/ImageReader.cpp
	impl/ffmpeg/PcmDecoder.cpp
	impl/ffmpeg/TagReader.cpp
	impl/ffmpeg/Transcoder.cpp
	impl/ffmpeg/Utils.cpp
	impl/taglib/AudioFileInfo.cpp
	impl/taglib/AudioFileInfoParser.cpp
	impl/taglib/ImageReader.cpp
	impl/taglib/TagReader.cpp
	impl/taglib/Utils.cpp
	impl/utils/PcmDecodeStreamer.cpp
	impl/AudioFileInfoParser.cpp
	impl/AudioOutput.cpp
	impl/PcmTypes.cpp
	impl/TagReader.cpp
	)

target_include_directories(lmsaudio INTERFACE
	include
	)

target_include_directories(lmsaudio PRIVATE
	include
	${AVCODEC_INCLUDE_DIR}
	${AVFORMAT_INCLUDE_DIR}
	${AVUTIL_INCLUDE_DIR}
	)

target_link_libraries(lmsaudio PUBLIC
	lmscore
	)

target_link_libraries(lmsaudio PRIVATE
	PkgConfig::LIBAV
	PkgConfig::Taglib
	PkgConfig::FFTW3f
	)

target_compile_definitions(lmsaudio PRIVATE
    $<$<BOOL:${PulseAudio_FOUND}>:LMS_HAVE_PULSEAUDIO>
    $<$<BOOL:${ALSA_FOUND}>:LMS_HAVE_ALSA>
	)

if (PulseAudio_FOUND)
    target_sources(lmsaudio PRIVATE
		impl/pulseaudio/AudioOutput.cpp
		impl/pulseaudio/AudioOutputStream.cpp
		impl/pulseaudio/Exception.cpp
		impl/pulseaudio/MainLoopScopedLock.cpp
	)

    target_link_libraries(lmsaudio PRIVATE
        PkgConfig::PulseAudio
    )
endif()

if (ALSA_FOUND)
    target_sources(lmsaudio PRIVATE
		impl/alsa/AudioOutput.cpp
		impl/alsa/AudioOutputStream.cpp
	)

    target_link_libraries(lmsaudio PRIVATE
        PkgConfig::ALSA
    )
endif()

if(BUILD_TESTING)
	add_subdirectory(test)
endif()

if (BUILD_BENCHMARKS)
	add_subdirectory(bench)
endif()
