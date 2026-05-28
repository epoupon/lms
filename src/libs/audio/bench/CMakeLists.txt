add_executable(bench-audio
	Audio.cpp
	)

if (OnnxRuntime_FOUND)
    target_sources(bench-audio PRIVATE
		MusicNNModel.cpp
		)
endif()

target_include_directories(bench-audio PRIVATE
	../impl
	)

target_link_libraries(bench-audio PRIVATE
	lmsaudio
	benchmark
	)
