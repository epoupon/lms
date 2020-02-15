
add_executable(lms-recommendation-features
	LmsRecommendationFeatures.cpp
	)

target_link_libraries(lms-recommendation-features PRIVATE
	lmsdatabase
	lmsrecommendation
	)

