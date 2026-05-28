pkg_check_modules(LIBAV IMPORTED_TARGET libavcodec libavutil libavformat libswresample)
pkg_check_modules(Taglib REQUIRED IMPORTED_TARGET taglib)
pkg_check_modules(PulseAudio IMPORTED_TARGET libpulse)
pkg_check_modules(ALSA IMPORTED_TARGET alsa)
pkg_check_modules(OnnxRuntime IMPORTED_TARGET libonnxruntime)

if (PulseAudio_FOUND OR ALSA_FOUND)
    message(STATUS "Audio output available (PulseAudio=${PulseAudio_FOUND}, ALSA=${ALSA_FOUND})")
else()
    message(STATUS "No audio output backend found")
endif()

add_library(lmsaudio STATIC
	impl/features/MelFilterBank.cpp
	impl/ffmpeg/AudioFile.cpp
	impl/ffmpeg/AudioFileInfo.cpp
	impl/ffmpeg/AudioFileInfoParser.cpp
	impl/ffmpeg/FFmpegTypes.cpp
	impl/ffmpeg/ImageReader.cpp
	impl/ffmpeg/PcmDecoder.cpp
	impl/ffmpeg/TagReader.cpp
	impl/ffmpeg/Transcoder.cpp
	impl/ffmpeg/Utils.cpp
	impl/musicnn/MusicNNEmbeddings.cpp
	impl/taglib/AudioFileInfo.cpp
	impl/taglib/AudioFileInfoParser.cpp
	impl/taglib/ImageReader.cpp
	impl/taglib/TagReader.cpp
	impl/taglib/Utils.cpp
	impl/utils/PcmDecodeStreamer.cpp
	impl/AudioFileInfoParser.cpp
	impl/AudioOutput.cpp
	impl/MusicNNEmbeddingExtractorCreator.cpp
	impl/PcmTypes.cpp
	impl/TagReader.cpp
	)

target_include_directories(lmsaudio INTERFACE
	include
	)

target_include_directories(lmsaudio PRIVATE
	include
	impl
	${AVCODEC_INCLUDE_DIR}
	${AVFORMAT_INCLUDE_DIR}
	${AVUTIL_INCLUDE_DIR}
	)

target_link_libraries(lmsaudio PUBLIC
	lmscore
	)

target_link_libraries(lmsaudio PRIVATE
	lmsmath
	PkgConfig::LIBAV
	PkgConfig::Taglib
	)

target_compile_definitions(lmsaudio PRIVATE
    $<$<BOOL:${PulseAudio_FOUND}>:LMS_HAVE_PULSEAUDIO>
    $<$<BOOL:${ALSA_FOUND}>:LMS_HAVE_ALSA>
	$<$<BOOL:${OnnxRuntime_FOUND}>:LMS_HAVE_ONNX_RUNTIME>
	)

# Should be safe enough for what we're doing
target_compile_options(lmsaudio PRIVATE
	$<$<NOT:$<CONFIG:Debug>>:-ffast-math>
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

if (OnnxRuntime_FOUND)
    target_sources(lmsaudio PRIVATE
		impl/musicnn/MusicNNEmbeddingExtractor.cpp
        impl/musicnn/MusicNNModel.cpp
    )
    target_link_libraries(lmsaudio PRIVATE PkgConfig::OnnxRuntime)
    message(STATUS "Using ONNX Runtime (${OnnxRuntime_VERSION})")

	install(DIRECTORY models DESTINATION share/lms)
endif()

if(BUILD_TESTING)
	add_subdirectory(test)
endif()

if (BUILD_BENCHMARKS)
	add_subdirectory(bench)
endif()
