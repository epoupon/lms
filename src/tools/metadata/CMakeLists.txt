
add_executable(lms-metadata
	LmsMetadata.cpp
	)

target_link_libraries(lms-metadata PRIVATE
	lmsmetadata
	lmsutils
	)

install(TARGETS lms-metadata DESTINATION bin)

