
add_library(lmsrecommendation STATIC
	impl/clusters/ClustersEngine.cpp
	impl/audio-similarity/features/AudioFeatureEngine.cpp
	impl/audio-similarity/features/AudioFeatureProvider.cpp
	impl/audio-similarity/musicnn/MusicNNEmbeddingEngine.cpp
	impl/audio-similarity/musicnn/MusicNNEmbeddingProvider.cpp
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
	lmsaudio
	lmsmath
	)

target_link_libraries(lmsrecommendation PUBLIC
	lmscore
	lmsdatabase
	)

# Should be safe enough for what we're doing
target_compile_options(lmsrecommendation PRIVATE
	$<$<NOT:$<CONFIG:Debug>>:-ffast-math>
	)
