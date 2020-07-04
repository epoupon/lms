
add_library(lmsav SHARED
	impl/AvInfo.cpp
	impl/AvTranscoder.cpp
	impl/AvTranscodeResourceHandler.cpp
	impl/AvTypes.cpp
	)

target_include_directories(lmsav INTERFACE
	include
	${AVFORMAT_INCLUDE_DIR}
	${AVUTIL_INCLUDE_DIR}
	)
#	${AVCODEC_INCLUDE_DIR}
#	${AVDEVICE_INCLUDE_DIR}

target_include_directories(lmsav PRIVATE
	include/

	${AVFORMAT_INCLUDE_DIR}
	${AVUTIL_INCLUDE_DIR}
	)
#	${AVCODEC_INCLUDE_DIR}
#	${AVDEVICE_INCLUDE_DIR}

# TODO make these private
target_link_libraries(lmsav PUBLIC
	lmsutils
	avformat
	avutil
	std::filesystem
	wt
	${AVFORMAT_LIBRARY}
	${AVUTIL_LIBRARY}
	)

install(TARGETS lmsav DESTINATION lib)

