
add_executable(lms-metadata
	LmsMetadata.cpp
	)

target_link_libraries(lms-metadata PRIVATE
	lmsmetadata
	lmscore
	Boost::program_options
	)

install(TARGETS lms-metadata DESTINATION bin)

