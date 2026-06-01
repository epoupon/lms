
add_library(lmsrecommendation STATIC
	impl/clusters/ClustersEngine.cpp
	impl/audio-similarity/musicnn/MusicNNEmbeddingEngine.cpp
	impl/audio-similarity/musicnn/MusicNNEmbeddingProvider.cpp
	impl/track-selection-constraints/SameArtistConstraint.cpp
	impl/track-selection-constraints/SameReleaseConstraint.cpp
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

if(BUILD_TESTING)
	add_subdirectory(test)
endif()
