pkg_check_modules(LIBAV IMPORTED_TARGET libavcodec libavutil libavformat)
pkg_check_modules(Taglib REQUIRED IMPORTED_TARGET taglib)

add_library(lmsaudio STATIC
	impl/ffmpeg/AudioFile.cpp
	impl/ffmpeg/AudioFileInfo.cpp
	impl/ffmpeg/ImageReader.cpp
	impl/ffmpeg/TagReader.cpp
	impl/ffmpeg/Transcoder.cpp
	impl/ffmpeg/Utils.cpp
	impl/taglib/AudioFileInfo.cpp
	impl/taglib/ImageReader.cpp
	impl/taglib/TagReader.cpp
	impl/taglib/Utils.cpp
	impl/AudioTypes.cpp
	impl/ImageReader.cpp
	impl/ParseAudioFileInfo.cpp
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
	std::filesystem
	)

target_link_libraries(lmsaudio PRIVATE
	PkgConfig::LIBAV
	PkgConfig::Taglib
	)
