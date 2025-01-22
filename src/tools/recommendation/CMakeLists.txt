
add_executable(lms-recommendation
	LmsRecommendation.cpp
	)

target_link_libraries(lms-recommendation PRIVATE
	lmsdatabase
	lmsrecommendation
	Boost::program_options
	)
