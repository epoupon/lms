include(GoogleTest)

add_executable(test-audio
	Audio.cpp
	AudioFeatures.cpp
	DeltaCalculator.cpp
	FFT.cpp
	MelFilterBank.cpp
	StatsAccumulator.cpp
	)

target_include_directories(test-audio PRIVATE
	../impl
	)

target_link_libraries(test-audio PRIVATE
	lmsaudio
	GTest::GTest
	)

if (NOT CMAKE_CROSSCOMPILING)
	gtest_discover_tests(test-audio)
endif()

