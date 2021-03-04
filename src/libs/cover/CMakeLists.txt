
add_library(lmscover SHARED
	impl/CoverArtGrabber.cpp
	)

target_include_directories(lmscover INTERFACE
	include
	)

target_include_directories(lmscover PRIVATE
	include
	impl
	)

target_link_libraries(lmscover PRIVATE
	lmsav
	)

target_link_libraries(lmscover PUBLIC
	lmsdatabase
	lmsutils
	std::filesystem
	)

if (IMAGE_LIBRARY STREQUAL STB)
	target_sources(lmscover PRIVATE
		impl/stb/JPEGImage.cpp
		impl/stb/RawImage.cpp
		)
	target_compile_options(lmscover PRIVATE "-DLMS_SUPPORT_IMAGE_STB")
	target_include_directories(lmscover PRIVATE ${STB_INCLUDE_DIR})
elseif (IMAGE_LIBRARY STREQUAL GraphicsMagick++)
	target_sources(lmscover PRIVATE
		impl/graphicsmagick/JPEGImage.cpp
		impl/graphicsmagick/RawImage.cpp
		)
	target_compile_options(lmscover PRIVATE "-DLMS_SUPPORT_IMAGE_GM")
	target_link_libraries(lmscover PRIVATE PkgConfig::GraphicsMagick++)
else ()
	message(FATAL_ERROR "Invalid IMAGE_LIBRARY provided")
endif()

install(TARGETS lmscover DESTINATION lib)

