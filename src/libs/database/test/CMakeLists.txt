
add_executable(test-database
	Artist.cpp
	ArtistInfo.cpp
	Artwork.cpp
	AuthToken.cpp
	Cluster.cpp
	Common.cpp
	DatabaseTest.cpp
	Directory.cpp
	Genre.cpp
	Grouping.cpp
	Image.cpp
	Language.cpp
	Listen.cpp
	Medium.cpp
	Migration.cpp
	Mood.cpp
	Movement.cpp
	PlayListFile.cpp
	Podcast.cpp
	RatedArtist.cpp
	RatedRelease.cpp
	RatedTrack.cpp
	Release.cpp
	ReleaseArtistLink.cpp
	ScanSettings.cpp
	StarredArtist.cpp
	StarredRelease.cpp
	StarredTrack.cpp
	Track.cpp
	TrackArtistLink.cpp
	Work.cpp
	TrackBookmark.cpp
	TrackEmbeddedImage.cpp
	TrackList.cpp
	TrackLyrics.cpp
	User.cpp
	)

target_link_libraries(test-database PRIVATE
	lmsdatabase
	GTest::GTest
	GTest::gtest_main
	)

if (NOT CMAKE_CROSSCOMPILING)
	gtest_discover_tests(test-database)
endif()

