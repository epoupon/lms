
add_library(lmsimage SHARED
	)

target_include_directories(lmsimage INTERFACE
	include
	)

target_include_directories(lmsimage PRIVATE
	include
	impl
	)

target_link_libraries(lmsimage PUBLIC
	lmsutils
	std::filesystem
	)

if (IMAGE_LIBRARY STREQUAL STB)
	target_sources(lmsimage PRIVATE
		impl/stb/JPEGImage.cpp
		impl/stb/RawImage.cpp
		)
	target_compile_options(lmsimage PRIVATE "-DLMS_SUPPORT_IMAGE_STB")
	target_include_directories(lmsimage PRIVATE ${STB_INCLUDE_DIR})
elseif (IMAGE_LIBRARY STREQUAL GraphicsMagick++)
	target_sources(lmsimage PRIVATE
		impl/graphicsmagick/JPEGImage.cpp
		impl/graphicsmagick/RawImage.cpp
		)
	target_compile_options(lmsimage PRIVATE "-DLMS_SUPPORT_IMAGE_GM")
	target_link_libraries(lmsimage PRIVATE PkgConfig::GraphicsMagick++)
else ()
	message(FATAL_ERROR "Invalid IMAGE_LIBRARY provided")
endif()

install(TARGETS lmsimage DESTINATION lib)

