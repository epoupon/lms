
add_library(lmsav SHARED
	impl/AudioFile.cpp
	impl/RawResourceHandlerCreator.cpp
	impl/Transcoder.cpp
	impl/TranscodingResourceHandler.cpp
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

install(TARGETS lmsav DESTINATION ${CMAKE_INSTALL_LIBDIR})

