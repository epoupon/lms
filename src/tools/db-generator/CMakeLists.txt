
add_executable(lms-db-generator
	LmsDbGenerator.cpp
	)

target_link_libraries(lms-db-generator PRIVATE
	lmsdatabase
	lmsutils
	Boost::program_options
	)
