add_library(lmsdatabase STATIC
	impl/objects/Artist.cpp
	impl/objects/ArtistInfo.cpp
	impl/objects/Artwork.cpp
	impl/objects/AuthToken.cpp
	impl/objects/Cluster.cpp
	impl/objects/Directory.cpp
	impl/objects/Image.cpp
	impl/objects/Listen.cpp
	impl/objects/MediaLibrary.cpp
	impl/objects/PlayListFile.cpp
	impl/objects/PlayQueue.cpp
	impl/objects/TrackArtistLink.cpp
	impl/objects/TrackFeatures.cpp
	impl/objects/TrackList.cpp
	impl/objects/RatedArtist.cpp
	impl/objects/RatedRelease.cpp
	impl/objects/RatedTrack.cpp
	impl/objects/Release.cpp
	impl/objects/ScanSettings.cpp
	impl/objects/StarredArtist.cpp
	impl/objects/StarredRelease.cpp
	impl/objects/StarredTrack.cpp
	impl/objects/Track.cpp
	impl/objects/TrackBookmark.cpp
	impl/objects/TrackEmbeddedImage.cpp
	impl/objects/TrackEmbeddedImageLink.cpp
	impl/objects/TrackLyrics.cpp
	impl/objects/UIState.cpp
	impl/objects/User.cpp
	impl/Db.cpp
	impl/IdType.cpp
	impl/Migration.cpp
	impl/Object.cpp
	impl/QueryPlanRecorder.cpp
	impl/Session.cpp
	impl/SqlQuery.cpp
	impl/Transaction.cpp
	impl/Types.cpp
	impl/Utils.cpp
	)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	target_sources(lmsdatabase PRIVATE impl/TransactionChecker.cpp)
endif()

target_include_directories(lmsdatabase INTERFACE
	include
	)

target_include_directories(lmsdatabase PRIVATE
	impl
	include
	)

target_link_libraries(lmsdatabase PRIVATE
	Wt::DboSqlite3
	)

target_link_libraries(lmsdatabase PUBLIC
	lmscore
	std::filesystem
	Wt::Dbo
	)

if(BUILD_TESTING)
	add_subdirectory(test)
endif()
