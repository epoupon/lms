
add_executable(lms-cover
	LmsCover.cpp
	)

target_link_libraries(lms-cover PRIVATE
	lmsservice-cover
	Boost::program_options
	)

