
add_library(lmsimage SHARED
	impl/SvgImage.cpp
	)

target_include_directories(lmsimage INTERFACE
	include
	)

target_include_directories(lmsimage PRIVATE
	include
	impl
	)

target_link_libraries(lmsimage PUBLIC
	lmscore
	std::filesystem
	)

set(IMAGE_LIBRARY "StbImage" CACHE STRING "Image library")
set_property(CACHE IMAGE_LIBRARY PROPERTY STRINGS "StbImage" "GraphicsMagick++")

if (${IMAGE_LIBRARY} STREQUAL "StbImage")
	find_package(StbImage REQUIRED)
	message(STATUS "Using StbImage (resize version ${STB_IMAGE_RESIZE_VERSION})")

	target_sources(lmsimage PRIVATE
		impl/stb/JPEGImage.cpp
		impl/stb/RawImage.cpp
	)
	target_compile_options(lmsimage PRIVATE "-DLMS_IMAGE_LIBRARY_STB_IMAGE")
	target_compile_options(lmsimage PRIVATE "-DLMS_IMAGE_LIBRARY_STB_RESIZE_VERSION=${STB_IMAGE_RESIZE_VERSION}")
	target_include_directories(lmsimage PRIVATE ${STB_IMAGE_INCLUDE_DIR})

elseif (IMAGE_LIBRARY STREQUAL "GraphicsMagick++")
	pkg_check_modules(GraphicsMagick++ REQUIRED IMPORTED_TARGET GraphicsMagick++)
	message(STATUS "Using GraphicsMagick++")

	target_sources(lmsimage PRIVATE
		impl/graphicsmagick/JPEGImage.cpp
		impl/graphicsmagick/RawImage.cpp
	)
	target_compile_options(lmsimage PRIVATE "-DLMS_IMAGE_LIBRARY_GM")
	target_link_libraries(lmsimage PRIVATE PkgConfig::GraphicsMagick++)

else ()
	message(FATAL_ERROR "Invalid image library")
endif ()

install(TARGETS lmsimage DESTINATION ${CMAKE_INSTALL_LIBDIR})

