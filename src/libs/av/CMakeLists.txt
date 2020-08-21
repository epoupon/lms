
add_library(lmsav SHARED
	impl/AvInfo.cpp
	impl/AvTranscoder.cpp
	impl/AvTranscodeResourceHandler.cpp
	impl/AvTypes.cpp
	)

target_include_directories(lmsav INTERFACE
	include
	)

target_include_directories(lmsav PRIVATE
	include/
	${AVCODEC_INCLUDE_DIR}
	${AVFORMAT_INCLUDE_DIR}
	${AVUTIL_INCLUDE_DIR}
	)

target_link_libraries(lmsav PUBLIC
	lmsutils
	avformat
	avutil
	std::filesystem
	wt
	)

target_link_libraries(lmsav PRIVATE
	${AVFORMAT_LIBRARY}
	${AVUTIL_LIBRARY}
	)

install(TARGETS lmsav DESTINATION lib)

