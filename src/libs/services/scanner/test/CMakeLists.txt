include(GoogleTest)

add_executable(test-scanner
	ArtistInfo.cpp
	AudioFileUtils.cpp
	Lyrics.cpp
	PlayList.cpp
	Scanner.cpp
	TrackMetadataParser.cpp
	)

target_include_directories(test-scanner PRIVATE
	../impl
	)

target_link_libraries(test-scanner PRIVATE
	lmsscanner
	lmsaudio
	GTest::GTest
	)

if (NOT CMAKE_CROSSCOMPILING)
	gtest_discover_tests(test-scanner)
endif()

