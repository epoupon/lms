
add_executable(lms-cover
	LmsCover.cpp
	)

target_link_libraries(lms-cover PRIVATE
	lmscore
	Boost::program_options
	)

