
add_library(lmsrecommendation SHARED
	impl/clusters/ClustersClassifier.cpp
	impl/features/FeaturesClassifierCache.cpp
	impl/features/FeaturesClassifier.cpp
	impl/features/FeaturesDefs.cpp
	impl/Engine.cpp
	)

target_include_directories(lmsrecommendation INTERFACE
	include
	)

target_include_directories(lmsrecommendation PRIVATE
	include
	)

target_link_libraries(lmsrecommendation PRIVATE
	lmsdatabase
	lmssom
	wt
	)

install(TARGETS lmsrecommendation DESTINATION lib)

