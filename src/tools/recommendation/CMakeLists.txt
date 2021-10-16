
add_executable(lms-recommendation
	LmsRecommendation.cpp
	)

target_link_libraries(lms-recommendation PRIVATE
	lmscore
	lmsrecommendation
	Boost::program_options
	)

install(TARGETS lms-recommendation DESTINATION bin)
