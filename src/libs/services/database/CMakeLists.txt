add_library(lmsdatabase SHARED
	impl/Artist.cpp
	impl/AuthToken.cpp
	impl/Cluster.cpp
	impl/Db.cpp
	impl/Listen.cpp
	impl/Migration.cpp
	impl/TrackArtistLink.cpp
	impl/TrackFeatures.cpp
	impl/TrackList.cpp
	impl/Release.cpp
	impl/ScanSettings.cpp
	impl/Session.cpp
	impl/StarredArtist.cpp
	impl/StarredRelease.cpp
	impl/StarredTrack.cpp
	impl/SqlQuery.cpp
	impl/Track.cpp
	impl/TrackBookmark.cpp
	impl/Types.cpp
	impl/User.cpp
	impl/Utils.cpp
	)

target_include_directories(lmsdatabase INTERFACE
	include
	)

target_include_directories(lmsdatabase PRIVATE
	include
	)

target_link_libraries(lmsdatabase PRIVATE
	Wt::DboSqlite3
	)

target_link_libraries(lmsdatabase PUBLIC
	lmsutils
	std::filesystem
	Wt::Dbo
	)

install(TARGETS lmsdatabase DESTINATION lib)

if(BUILD_TESTING)
	add_subdirectory(test)
endif()
