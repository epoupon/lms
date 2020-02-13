
add_library(lmsrecommendation SHARED
	impl/Engine.cpp
	impl/ProviderCreator.cpp
	impl/features/som/DataNormalizer.cpp
	impl/features/som/Network.cpp
	)

target_include_directories(lmsrecommendation INTERFACE
	include
	)

target_include_directories(lmsrecommendation PRIVATE
	include
	)

target_link_libraries(lmsrecommendation PRIVATE
	lmsdatabase
	)

target_link_libraries(lmsrecommendation PUBLIC
	)

install(TARGETS lmsrecommendation DESTINATION lib)

