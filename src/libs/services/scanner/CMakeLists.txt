
add_library(lmsscanner SHARED
	impl/ScannerService.cpp
	impl/ScannerStats.cpp
	impl/ScanStepCheckDuplicatedDbFiles.cpp
	impl/ScanStepCompact.cpp
	impl/ScanStepComputeClusterStats.cpp
	impl/ScanStepDiscoverFiles.cpp
	impl/ScanStepOptimize.cpp
	impl/ScanStepRemoveOrphanDbFiles.cpp
	impl/ScanStepScanArtistImages.cpp
	impl/ScanStepScanAudioFiles.cpp
	)

target_include_directories(lmsscanner INTERFACE
	include
	)

target_include_directories(lmsscanner PRIVATE
	include
	)

target_link_libraries(lmsscanner PRIVATE
	lmscore
	lmsdatabase
	lmsimage
	lmsmetadata
	lmsrecommendation
	)

target_link_libraries(lmsscanner PUBLIC
	std::filesystem
	Wt::Wt
	)

install(TARGETS lmsscanner DESTINATION ${CMAKE_INSTALL_LIBDIR})

