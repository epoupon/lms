
add_executable(lms-cover
	LmsCover.cpp
	)

target_link_libraries(lms-cover PRIVATE
	lmscover
	Boost::program_options
	)

