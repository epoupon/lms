
add_library(lmsrecommendation SHARED
	impl/Engine.cpp
	impl/ProviderCreator.cpp
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
	)

install(TARGETS lmsrecommendation DESTINATION lib)

