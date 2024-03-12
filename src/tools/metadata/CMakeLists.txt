
add_executable(lms-metadata
	LmsMetadata.cpp
	)

target_link_libraries(lms-metadata PRIVATE
	lmsmetadata
	lmscore
	)

install(TARGETS lms-metadata DESTINATION bin)

