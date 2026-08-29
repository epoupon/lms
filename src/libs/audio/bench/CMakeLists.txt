add_executable(bench-audio
	Audio.cpp
	AudioFileInfoParser.cpp
	../test/TestData.cpp
	)

if (OnnxRuntime_FOUND)
    target_sources(bench-audio PRIVATE
		MusicNNModel.cpp
		)

	target_compile_definitions(bench-audio PRIVATE
		LMS_MUSICNN_DEFAULT_MODEL_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../models/MSD_musicnn_embedding.onnx"
		)
endif()

target_compile_definitions(bench-audio PRIVATE
	LMS_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../test/data"
	)

target_include_directories(bench-audio PRIVATE
	../impl
	../test
	)

target_link_libraries(bench-audio PRIVATE
	lmsaudio
	benchmark
	)
