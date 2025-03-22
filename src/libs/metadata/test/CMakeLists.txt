include(GoogleTest)

add_executable(test-metadata
	ArtistInfo.cpp
	Lyrics.cpp
	Metadata.cpp
	AudioFileParser.cpp
	PlayList.cpp
	Utils.cpp
	)

target_include_directories(test-metadata PRIVATE
	../impl
	)

target_link_libraries(test-metadata PRIVATE
	lmsmetadata
	GTest::GTest
	)

if (NOT CMAKE_CROSSCOMPILING)
	gtest_discover_tests(test-metadata)
endif()

