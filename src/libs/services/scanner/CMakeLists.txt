add_library(lmsscanner STATIC
	impl/helpers/ArtistHelpers.cpp
	impl/scanners/ArtistInfoFileScanner.cpp
	impl/scanners/AudioFileScanOperation.cpp
	impl/scanners/FileScanOperationBase.cpp
	impl/scanners/AudioFileScanner.cpp
	impl/scanners/ImageFileScanner.cpp
	impl/scanners/LyricsFileScanner.cpp
	impl/scanners/PlayListFileScanner.cpp
	impl/scanners/Utils.cpp
	impl/steps/JobQueue.cpp
	impl/steps/ScanErrorLogger.cpp
	impl/steps/ScanStepArtistReconciliation.cpp
	impl/steps/ScanStepAssociateArtistImages.cpp
	impl/steps/ScanStepAssociateExternalLyrics.cpp
	impl/steps/ScanStepAssociatePlayListTracks.cpp
	impl/steps/ScanStepAssociateReleaseImages.cpp
	impl/steps/ScanStepAssociateTrackImages.cpp
	impl/steps/ScanStepBase.cpp
	impl/steps/ScanStepCheckForDuplicatedFiles.cpp
	impl/steps/ScanStepCheckForRemovedFiles.cpp
	impl/steps/ScanStepCompact.cpp
	impl/steps/ScanStepComputeClusterStats.cpp
	impl/steps/ScanStepOptimize.cpp
	impl/steps/ScanStepRemoveOrphanedDbEntries.cpp
	impl/steps/ScanStepScanFiles.cpp
	impl/steps/ScanStepUpdateLibraryFields.cpp
	impl/FileScanners.cpp
	impl/ScannerService.cpp
	impl/ScannerServiceTraceLogger.cpp
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
