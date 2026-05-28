include(GoogleTest)

add_executable(test-audio
	MelFilterBank.cpp
	MusicNNEmbeddings.cpp
	PcmSpectralFrameDecoder.cpp
	)

if (OnnxRuntime_FOUND)
    target_sources(test-audio PRIVATE
		MusicNNModel.cpp
		)
endif()

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

