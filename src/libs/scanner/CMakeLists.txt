
add_library(lmsscanner SHARED
	impl/AcousticBrainzUtils.cpp
	impl/MediaScanner.cpp
	impl/MediaScannerStats.cpp
	)

target_include_directories(lmsscanner INTERFACE
	include
	)

target_include_directories(lmsscanner PRIVATE
	include
	)

target_link_libraries(lmsscanner PRIVATE
	lmsdatabase
	lmsmetadata
	lmsutils
	)

target_link_libraries(lmsscanner PUBLIC
	std::filesystem
	wt
	)

install(TARGETS lmsscanner DESTINATION lib)

