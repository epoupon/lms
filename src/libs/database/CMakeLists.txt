
add_library(lmsdatabase SHARED
	impl/Artist.cpp
	impl/Cluster.cpp
	impl/Db.cpp
	impl/TrackArtistLink.cpp
	impl/TrackFeatures.cpp
	impl/TrackList.cpp
	impl/Release.cpp
	impl/ScanSettings.cpp
	impl/Session.cpp
	impl/SqlQuery.cpp
	impl/Track.cpp
	impl/TrackBookmark.cpp
	impl/User.cpp
	)

target_include_directories(lmsdatabase INTERFACE
	include
	)

target_include_directories(lmsdatabase PRIVATE
	include
	)

target_link_libraries(lmsdatabase PRIVATE
	wtdbosqlite3
	)

target_link_libraries(lmsdatabase PUBLIC
	lmsutils
	std::filesystem
	wtdbo
	)

install(TARGETS lmsdatabase DESTINATION lib)

