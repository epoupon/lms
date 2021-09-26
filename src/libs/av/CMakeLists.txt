
add_library(lmsav SHARED
	impl/AudioFile.cpp
	impl/Transcoder.cpp
	impl/TranscodeResourceHandler.cpp
	impl/Types.cpp
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
	lmsutils
	std::filesystem
	)

target_link_libraries(lmsav PRIVATE
	PkgConfig::LIBAV
	)

install(TARGETS lmsav DESTINATION lib)

