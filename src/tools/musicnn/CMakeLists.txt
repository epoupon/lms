add_executable(lms-musicnn
	LmsMusicNN.cpp
	)

target_link_libraries(lms-musicnn PRIVATE
	lmsaudio
	lmscore
	Boost::program_options
	)
