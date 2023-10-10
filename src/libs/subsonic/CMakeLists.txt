
add_library(lmssubsonic SHARED
	impl/entrypoints/AlbumSongLists.cpp
	impl/entrypoints/Bookmarks.cpp
	impl/entrypoints/Browsing.cpp
	impl/entrypoints/MediaAnnotation.cpp
	impl/entrypoints/MediaLibraryScanning.cpp
	impl/entrypoints/MediaRetrieval.cpp
	impl/entrypoints/Playlists.cpp
	impl/entrypoints/Searching.cpp
	impl/entrypoints/System.cpp
	impl/entrypoints/UserManagement.cpp
	impl/responses/Album.cpp
	impl/responses/Artist.cpp
	impl/responses/Bookmark.cpp
	impl/responses/Contributor.cpp
	impl/responses/DiscTitle.cpp
	impl/responses/ItemGenre.cpp
	impl/responses/Genre.cpp
	impl/responses/Playlist.cpp
	impl/responses/ReplayGain.cpp
	impl/responses/Song.cpp
	impl/responses/User.cpp
	impl/ProtocolVersion.cpp
	impl/ParameterParsing.cpp
	impl/SubsonicId.cpp
	impl/SubsonicResource.cpp
	impl/SubsonicResponse.cpp
	impl/Utils.cpp
	)

target_include_directories(lmssubsonic INTERFACE
	include
	)

target_include_directories(lmssubsonic PRIVATE
	impl
	include
	)

target_link_libraries(lmssubsonic PRIVATE
	lmsauth
	lmsav
	lmsdatabase
	lmsrecommendation
	lmsscanner
	lmsscrobbling
	lmsservice-cover
	lmsutils
	std::filesystem
	)

target_link_libraries(lmssubsonic PUBLIC
	Wt::Wt
	)

install(TARGETS lmssubsonic DESTINATION lib)

