
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
	${GRAPHICSMAGICKXX_LIBRARIES}
	)

target_link_libraries(lmscover PUBLIC
	lmsdatabase
	std::filesystem
	)

install(TARGETS lmscover DESTINATION lib)

