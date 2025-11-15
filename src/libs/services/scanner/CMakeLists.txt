add_library(lmsscanner STATIC
	impl/helpers/ArtistHelpers.cpp
	impl/scanners/artistinfo/ArtistInfoParser.cpp
	impl/scanners/artistinfo/ArtistInfoFileScanner.cpp
	impl/scanners/audiofile/AudioFileScanOperation.cpp
	impl/scanners/audiofile/AudioFileScanner.cpp
	impl/scanners/audiofile/TrackMetadataParser.cpp
	impl/scanners/audiofile/Utils.cpp
	impl/scanners/lyrics/LyricsFileScanner.cpp
	impl/scanners/lyrics/LyricsParser.cpp
	impl/scanners/playlist/PlayListFileScanner.cpp
	impl/scanners/playlist/PlayListParser.cpp
	impl/scanners/FileScanOperationBase.cpp
	impl/scanners/ImageFileScanner.cpp
	impl/scanners/Utils.cpp
	impl/steps/JobQueue.cpp
	impl/steps/ScanErrorLogger.cpp
	impl/steps/ScanStepArtistReconciliation.cpp
	impl/steps/ScanStepAssociateArtistImages.cpp
	impl/steps/ScanStepAssociateExternalLyrics.cpp
	impl/steps/ScanStepAssociateMediumImages.cpp
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
	lmsaudio
	lmsimage
	lmsrecommendation
	pugixml::pugixml
	)

target_link_libraries(lmsscanner PUBLIC
	lmsdatabase
	std::filesystem
	Wt::Wt
	)

if(BUILD_TESTING)
	add_subdirectory(test)
endif()

if (BUILD_BENCHMARKS)
	add_subdirectory(bench)
endif()
