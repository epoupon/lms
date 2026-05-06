add_executable(bench-audio
	Audio.cpp
	DeltaCalculator.cpp
	)

target_include_directories(bench-audio PRIVATE
	../impl
	)

target_link_libraries(bench-audio PRIVATE
	lmsaudio
	benchmark
	)
