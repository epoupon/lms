include(GoogleTest)

add_executable(test-audio
	AudioFeatures.cpp
	ChromaCalculator.cpp
	DeltaCalculator.cpp
	MelFilterBank.cpp
	MfccCalculator.cpp
	SpectralFeatureCalculator.cpp
	ZeroCrossingRateCalculator.cpp
	)

target_include_directories(test-audio PRIVATE
	../impl
	)

target_link_libraries(test-audio PRIVATE
	lmsaudio
	lmsmath
	GTest::GTest
	GTest::gtest_main
	)

target_compile_options(test-audio PRIVATE
	$<$<NOT:$<CONFIG:Debug>>:-ffast-math>
	)

if (NOT CMAKE_CROSSCOMPILING)
	gtest_discover_tests(test-audio)
endif()

