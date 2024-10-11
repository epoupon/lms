
add_library(lmsartwork SHARED
	impl/ImageCache.cpp
	impl/ArtworkService.cpp
	)

target_include_directories(lmsartwork INTERFACE
	include
	)

target_include_directories(lmsartwork PRIVATE
	include
	impl
	)

target_link_libraries(lmsartwork PRIVATE
	lmsav
	lmsimage
	)

target_link_libraries(lmsartwork PUBLIC
	lmsdatabase
	lmsimage
	lmscore
	std::filesystem
	)

install(TARGETS lmsartwork DESTINATION ${CMAKE_INSTALL_LIBDIR})

