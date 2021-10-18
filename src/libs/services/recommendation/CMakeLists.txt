
add_library(lmsrecommendation SHARED
	impl/clusters/ClustersEngine.cpp
	impl/features/FeaturesEngineCache.cpp
	impl/features/FeaturesEngine.cpp
	impl/features/FeaturesDefs.cpp
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
	lmsdatabase
	lmssom
	std::filesystem
	)

install(TARGETS lmsrecommendation DESTINATION lib)

