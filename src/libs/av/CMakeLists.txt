
add_library(lmsav SHARED
	impl/AvInfo.cpp
	impl/AvTranscoder.cpp
	impl/AvTranscodeResourceHandler.cpp
	impl/AvTypes.cpp
	)

# TODO make av libs private
target_include_directories(lmsav INTERFACE
	include
	${AVCODEC_INCLUDE_DIR}
	${AVFORMAT_INCLUDE_DIR}
	${AVUTIL_INCLUDE_DIR}
	)

target_include_directories(lmsav PRIVATE
	include/
	)

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

