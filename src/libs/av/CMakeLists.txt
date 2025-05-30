pkg_check_modules(LIBAV IMPORTED_TARGET libavcodec libavutil libavformat)

add_library(lmsav STATIC
	impl/AudioFile.cpp
	impl/Transcoder.cpp
	)

target_include_directories(lmsav INTERFACE
	include
	)

target_include_directories(lmsav PRIVATE
	include
	${AVCODEC_INCLUDE_DIR}
	${AVFORMAT_INCLUDE_DIR}
	${AVUTIL_INCLUDE_DIR}
	)

target_link_libraries(lmsav PUBLIC
	lmscore
	std::filesystem
	)

target_link_libraries(lmsav PRIVATE
	PkgConfig::LIBAV
	)
