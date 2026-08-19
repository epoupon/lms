
add_executable(lms-db-generator
	LmsDbGenerator.cpp
	)

target_link_libraries(lms-db-generator PRIVATE
	lmsaudio
	lmsdatabase
	lmscore
	Boost::program_options
	)
