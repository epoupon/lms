
add_library(lmsscanner SHARED
	impl/metadata/AvFormat.cpp
	impl/metadata/TagLibParser.cpp
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
	lmsav
	lmsdatabase
	lmsutils
	tag
	)

target_link_libraries(lmsscanner PUBLIC
	wt
	)

install(TARGETS lmsscanner DESTINATION lib)

