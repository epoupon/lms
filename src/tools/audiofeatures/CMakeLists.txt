
add_executable(lms-audiofeatures
	LmsAudioFeatures.cpp
	)

target_link_libraries(lms-audiofeatures PRIVATE
	lmsaudio
	lmscore
	Boost::program_options
	)
