
add_library(lmsimage STATIC
	impl/EncodedImage.cpp
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

set(LMS_IMAGE_BACKEND "stb" CACHE STRING "Image library")
set_property(CACHE LMS_IMAGE_BACKEND PROPERTY STRINGS "stb" "graphicsmagick")

if (${LMS_IMAGE_BACKEND} STREQUAL "stb")
	find_package(StbImage REQUIRED)
	message(STATUS "Using stb (resize version ${STB_IMAGE_RESIZE_VERSION})")

	target_sources(lmsimage PRIVATE
		impl/stb/Image.cpp
		impl/stb/Exception.cpp
		impl/stb/RawImage.cpp
		impl/stb/StbImage.cpp
		impl/stb/StbImageResize.cpp
		impl/stb/StbImageWrite.cpp
	)

	set_property(SOURCE impl/stb/StbImage.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
	set_property(SOURCE impl/stb/StbImageResize.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
	set_property(SOURCE impl/stb/StbImageWrite.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)

	target_compile_options(lmsimage PRIVATE "-DSTB_IMAGE_RESIZE_VERSION=${STB_IMAGE_RESIZE_VERSION}")
	target_include_directories(lmsimage PRIVATE ${STB_IMAGE_INCLUDE_DIR})

elseif (${LMS_IMAGE_BACKEND} STREQUAL "graphicsmagick")
	pkg_check_modules(GraphicsMagick++ REQUIRED IMPORTED_TARGET GraphicsMagick++)
	message(STATUS "Using graphicsmagick")

	target_sources(lmsimage PRIVATE
		impl/graphicsmagick/Image.cpp
		impl/graphicsmagick/RawImage.cpp
	)
	target_link_libraries(lmsimage PRIVATE PkgConfig::GraphicsMagick++)

else ()
	message(FATAL_ERROR "Invalid image library")
endif ()
