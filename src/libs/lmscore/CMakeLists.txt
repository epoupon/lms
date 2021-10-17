
add_library(lmscore SHARED
	impl/CoverService.cpp
	)

target_include_directories(lmscore INTERFACE
	include
	)

target_include_directories(lmscore PRIVATE
	include
	impl
	)

target_link_libraries(lmscore PRIVATE
	lmsav
	lmsimage
	)

target_link_libraries(lmscore PUBLIC
	lmsdatabase
	lmsimage
	lmsutils
	std::filesystem
	)

install(TARGETS lmscore DESTINATION lib)

