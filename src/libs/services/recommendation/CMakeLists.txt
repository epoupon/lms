
add_library(lmsrecommendation STATIC
	impl/clusters/ClustersEngine.cpp
	impl/features/FeaturesEngineCache.cpp
	impl/features/FeaturesEngine.cpp
	impl/features/FeaturesDefs.cpp
	impl/playlist-constraints/ConsecutiveArtists.cpp
	impl/playlist-constraints/ConsecutiveReleases.cpp
	impl/playlist-constraints/DuplicateTracks.cpp
	impl/PlaylistGeneratorService.cpp
	impl/RecommendationService.cpp
	)

target_include_directories(lmsrecommendation INTERFACE
	include
	)

target_include_directories(lmsrecommendation PRIVATE
	impl
	include
	)

target_link_libraries(lmsrecommendation PRIVATE

	lmssom
	std::filesystem
	)

target_link_libraries(lmsrecommendation PUBLIC
	lmsdatabase
	)
