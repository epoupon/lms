
add_executable(test-database
	AuthToken.cpp
	Artist.cpp
	Cluster.cpp
	Common.cpp
	DatabaseTest.cpp
	Directory.cpp
	Image.cpp
	Listen.cpp
	Migration.cpp
	PlayListFile.cpp
	RatedArtist.cpp
	RatedRelease.cpp
	RatedTrack.cpp
	Release.cpp
	StarredArtist.cpp
	StarredRelease.cpp
	StarredTrack.cpp
	Track.cpp
	TrackBookmark.cpp
	TrackFeatures.cpp
	TrackList.cpp
	TrackLyrics.cpp
	User.cpp
	)

target_link_libraries(test-database PRIVATE
	lmsdatabase
	GTest::GTest
	)

if (NOT CMAKE_CROSSCOMPILING)
	gtest_discover_tests(test-database)
endif()

