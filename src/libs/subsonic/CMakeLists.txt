
add_library(lmssubsonic STATIC
	impl/endpoints/AlbumSongLists.cpp
	impl/endpoints/Bookmarks.cpp
	impl/endpoints/Browsing.cpp
	impl/endpoints/MediaAnnotation.cpp
	impl/endpoints/MediaLibraryScanning.cpp
	impl/endpoints/MediaRetrieval.cpp
	impl/endpoints/Playlists.cpp
	impl/endpoints/Searching.cpp
	impl/endpoints/System.cpp
	impl/endpoints/UserManagement.cpp
	impl/responses/Album.cpp
	impl/responses/AlbumInfo.cpp
	impl/responses/Artist.cpp
	impl/responses/Bookmark.cpp
	impl/responses/Contributor.cpp
	impl/responses/DiscTitle.cpp
	impl/responses/ItemDate.cpp
	impl/responses/ItemGenre.cpp
	impl/responses/Genre.cpp
	impl/responses/Lyrics.cpp
	impl/responses/Playlist.cpp
	impl/responses/RecordLabel.cpp
	impl/responses/ReplayGain.cpp
	impl/responses/Song.cpp
	impl/responses/User.cpp
	impl/CoverArtId.cpp
	impl/ResponseFormat.cpp
	impl/ProtocolVersion.cpp
	impl/ParameterParsing.cpp
	impl/SubsonicId.cpp
	impl/SubsonicResource.cpp
	impl/SubsonicResponse.cpp
	)

target_include_directories(lmssubsonic INTERFACE
	include
	)

target_include_directories(lmssubsonic PRIVATE
	impl
	include
	)

target_link_libraries(lmssubsonic PRIVATE
	lmsartwork
	lmsauth
	lmsav
	lmsdatabase
	lmsfeedback
	lmsrecommendation
	lmsscanner
	lmsscrobbling
	lmstranscoding
	lmscore
	std::filesystem
	)

target_link_libraries(lmssubsonic PUBLIC
	Wt::Wt
	)

if (BUILD_BENCHMARKS)
       add_subdirectory(bench)
endif()