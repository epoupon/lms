
add_library(lmscover SHARED
	impl/CoverArtGrabber.cpp
	impl/Image.cpp
	)

target_include_directories(lmscover INTERFACE
	include
	)

target_include_directories(lmscover PRIVATE
	include
	${IMAGEMAGICKXX_INCLUDE_DIRS}
	)

target_compile_options(lmscover PRIVATE
	${IMAGEMAGICKXX_CFLAGS_OTHER}
	)

target_link_libraries(lmscover PRIVATE
	lmsav
	lmsdatabase
	${IMAGEMAGICKXX_LIBRARIES}
	)

install(TARGETS lmscover DESTINATION lib)

