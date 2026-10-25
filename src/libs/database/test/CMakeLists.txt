
add_executable(test-database
	Artist.cpp
	ArtistFeedback.cpp
	ArtistInfo.cpp
	Artwork.cpp
	AuthToken.cpp
	Cluster.cpp
	Common.cpp
	DatabaseTest.cpp
	Directory.cpp
	FilterExpr.cpp
	FilterExprCompiler.cpp
	Genre.cpp
	Grouping.cpp
	Image.cpp
	JoinSet.cpp
	Language.cpp
	Listen.cpp
	ListenBackendSync.cpp
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
	ReleaseFeedback.cpp
	ScanSettings.cpp
	SqlFragment.cpp
	Track.cpp
	TrackArtistLink.cpp
	TrackBookmark.cpp
	TrackEmbeddedImage.cpp
	TrackFeedback.cpp
	TrackList.cpp
	TrackLyrics.cpp
	User.cpp
	Work.cpp
	)

target_include_directories(test-database PRIVATE
	../impl
	)

target_link_libraries(test-database PRIVATE
	lmsdatabasetestutils
	GTest::GTest
	GTest::gtest_main
	)

if (NOT CMAKE_CROSSCOMPILING)
	gtest_discover_tests(test-database)
endif()

