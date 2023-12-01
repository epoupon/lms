
add_executable(test-database
	Artist.cpp
	Cluster.cpp
	Common.cpp
	DatabaseTest.cpp
	Listen.cpp
	Release.cpp
	StarredArtist.cpp
	StarredRelease.cpp
	StarredTrack.cpp
	Track.cpp
	TrackBookmark.cpp
	TrackFeatures.cpp
	TrackList.cpp
	)

target_link_libraries(test-database PRIVATE
	lmsdatabase
	GTest::GTest
	)

if (NOT CMAKE_CROSSCOMPILING)
	gtest_discover_tests(test-database)
endif()

