
add_executable(lms-audioinfo
	LmsAudioInfo.cpp
	)

target_link_libraries(lms-audioinfo PRIVATE
	lmsaudio
	lmscore
	Boost::program_options
	)

install(TARGETS lms-audioinfo DESTINATION bin)
