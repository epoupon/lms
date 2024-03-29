
add_library(lmsservice-cover SHARED
	impl/ImageCache.cpp
	impl/CoverService.cpp
	)

target_include_directories(lmsservice-cover INTERFACE
	include
	)

target_include_directories(lmsservice-cover PRIVATE
	include
	impl
	)

target_link_libraries(lmsservice-cover PRIVATE
	lmsav
	lmsimage
	)

target_link_libraries(lmsservice-cover PUBLIC
	lmsdatabase
	lmsimage
	lmscore
	std::filesystem
	)

install(TARGETS lmsservice-cover DESTINATION ${CMAKE_INSTALL_LIBDIR})

