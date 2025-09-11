add_executable(test-podcast
	PodcastParser.cpp
	PodcastService.cpp
	)

target_link_libraries(test-podcast PRIVATE
	lmscore
	lmspodcast
	GTest::GTest
	)

target_include_directories(test-podcast PRIVATE
	../impl
	)

if (NOT CMAKE_CROSSCOMPILING)
	gtest_discover_tests(test-podcast)
endif()

