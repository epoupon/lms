
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
	)

# TODO make these private
target_link_libraries(lmsav PUBLIC
	lmsutils
	avformat
	avutil
	wt
	)

install(TARGETS lmsav DESTINATION lib)

