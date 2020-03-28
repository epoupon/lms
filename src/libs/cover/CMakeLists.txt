
add_library(lmscover SHARED
	impl/CoverArtGrabber.cpp
	impl/Image.cpp
	)

target_include_directories(lmscover INTERFACE
	include
	)

target_include_directories(lmscover PRIVATE
	include
	${GRAPHICSMAGICKXX_INCLUDE_DIRS}
	)

target_compile_options(lmscover PRIVATE
	${GRAPHICSMAGICKXX_CFLAGS_OTHER}
	)

target_link_libraries(lmscover PRIVATE
	lmsav
	lmsdatabase
	${GRAPHICSMAGICKXX_LIBRARIES}
	)

install(TARGETS lmscover DESTINATION lib)

