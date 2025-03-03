
add_library(lmsartwork STATIC
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
	lmsimage
	lmsmetadata
	)

target_link_libraries(lmsartwork PUBLIC
	lmsdatabase
	lmsimage
	lmscore
	std::filesystem
	)
