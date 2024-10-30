
add_library(lmsscanner SHARED
	impl/FileScanQueue.cpp
	impl/ScannerService.cpp
	impl/ScannerStats.cpp
	impl/ScanStepAssociateArtistImages.cpp
	impl/ScanStepAssociateExternalLyrics.cpp
	impl/ScanStepAssociateReleaseImages.cpp
	impl/ScanStepCheckForDuplicatedFiles.cpp
	impl/ScanStepCheckForRemovedFiles.cpp
	impl/ScanStepCompact.cpp
	impl/ScanStepComputeClusterStats.cpp
	impl/ScanStepDiscoverFiles.cpp
	impl/ScanStepOptimize.cpp
	impl/ScanStepRemoveOrphanedDbEntries.cpp
	impl/ScanStepScanFiles.cpp
	impl/ScanStepUpdateLibraryFields.cpp
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

