
add_library(liblms SHARED
	impl/api/subsonic/SubsonicId.cpp
	impl/api/subsonic/SubsonicResource.cpp
	impl/api/subsonic/SubsonicResponse.cpp
	impl/auth/AuthTokenService.cpp
	impl/auth/PasswordService.cpp
	impl/auth/LoginThrottler.cpp
	impl/av/AvInfo.cpp
	impl/av/AvTranscoder.cpp
	impl/av/AvTypes.cpp
	impl/cover/CoverArtGrabber.cpp
	impl/cover/Image.cpp
	impl/database/Artist.cpp
	impl/database/Cluster.cpp
	impl/database/Db.cpp
	impl/database/TrackArtistLink.cpp
	impl/database/TrackFeatures.cpp
	impl/database/TrackList.cpp
	impl/database/Release.cpp
	impl/database/ScanSettings.cpp
	impl/database/Session.cpp
	impl/database/SessionPool.cpp
	impl/database/SqlQuery.cpp
	impl/database/Track.cpp
	impl/database/TrackBookmark.cpp
	impl/database/User.cpp
	impl/metadata/AvFormat.cpp
	impl/metadata/TagLibParser.cpp
	impl/scanner/MediaScanner.cpp
	impl/scanner/MediaScannerStats.cpp
	impl/recommendation/Engine.cpp
	impl/recommendation/ProviderCreator.cpp
	impl/recommendation/features/som/DataNormalizer.cpp
	impl/recommendation/features/som/Network.cpp
	impl/utils/Config.cpp
	impl/utils/Logger.cpp
	impl/utils/NetAddress.cpp
	impl/utils/Path.cpp
	impl/utils/Random.cpp
	impl/utils/StreamLogger.cpp
	impl/utils/String.cpp
	impl/utils/UUID.cpp
	impl/utils/WtLogger.cpp
	)

target_include_directories(liblms INTERFACE
	${CMAKE_CURRENT_SOURCE_DIR}/include
	)

target_include_directories(liblms PRIVATE
	include/
	${IMAGEMAGICKXX_INCLUDE_DIRS}
	)

target_compile_options(liblms PRIVATE
	${IMAGEMAGICKXX_CFLAGS_OTHER}
	)

target_link_libraries(liblms PRIVATE
	${IMAGEMAGICKXX_LIBRARIES}
	avformat
	avutil
	config++
	tag
	stdc++fs
	wtdbosqlite3
	)

target_link_libraries(liblms PUBLIC
	pthread
	boost_system
	wtdbo
	)

install(TARGETS liblms DESTINATION lib)

