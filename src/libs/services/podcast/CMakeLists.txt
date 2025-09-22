add_library(lmspodcast STATIC
	impl/steps/CheckForMissingFilesStep.cpp
	impl/steps/ClearTmpDirectoryStep.cpp
	impl/steps/DownloadEpisodeArtworksStep.cpp
	impl/steps/DownloadEpisodesStep.cpp
	impl/steps/DownloadPodcastArtworksStep.cpp
	impl/steps/RefreshPodcastsStep.cpp
	impl/steps/RemoveEpisodesStep.cpp
	impl/steps/RemovePodcastsStep.cpp
	impl/steps/Utils.cpp
	impl/Executor.cpp
	impl/PodcastParsing.cpp
	impl/PodcastService.cpp
	)

target_include_directories(lmspodcast INTERFACE
	include
	)

target_include_directories(lmspodcast PRIVATE
	include
	impl
	)

target_link_libraries(lmspodcast PRIVATE
	lmscore
	lmsimage
	pugixml::pugixml
	)

target_link_libraries(lmspodcast PUBLIC
	lmsdatabase
	)

if(BUILD_TESTING)
	add_subdirectory(test)
endif()
