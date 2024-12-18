add_library(lmsscanner SHARED
	impl/scanners/AudioFileScanner.cpp
	impl/scanners/ImageFileScanner.cpp
	impl/scanners/LyricsFileScanner.cpp
	impl/scanners/PlayListFileScanner.cpp
	impl/scanners/Utils.cpp
	impl/steps/FileScanQueue.cpp
	impl/steps/ScanStepAssociateArtistImages.cpp
	impl/steps/ScanStepAssociateExternalLyrics.cpp
	impl/steps/ScanStepAssociatePlayListTracks.cpp
	impl/steps/ScanStepAssociateReleaseImages.cpp
	impl/steps/ScanStepCheckForDuplicatedFiles.cpp
	impl/steps/ScanStepCheckForRemovedFiles.cpp
	impl/steps/ScanStepCompact.cpp
	impl/steps/ScanStepComputeClusterStats.cpp
	impl/steps/ScanStepDiscoverFiles.cpp
	impl/steps/ScanStepOptimize.cpp
	impl/steps/ScanStepRemoveOrphanedDbEntries.cpp
	impl/steps/ScanStepScanFiles.cpp
	impl/steps/ScanStepUpdateLibraryFields.cpp
	impl/ScannerService.cpp
	impl/ScannerStats.cpp
	)

target_include_directories(lmsscanner INTERFACE
	include
	)

target_include_directories(lmsscanner PRIVATE
	include
	impl
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

