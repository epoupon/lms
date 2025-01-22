
add_library(lmsscrobbling STATIC
	impl/internal/InternalBackend.cpp
	impl/listenbrainz/ListenBrainzBackend.cpp
	impl/listenbrainz/ListenTypes.cpp
	impl/listenbrainz/ListensParser.cpp
	impl/listenbrainz/ListensSynchronizer.cpp
	impl/listenbrainz/Utils.cpp
	impl/ScrobblingService.cpp
	)

target_include_directories(lmsscrobbling INTERFACE
	include
	)

target_include_directories(lmsscrobbling PRIVATE
	include
	impl
	)

target_link_libraries(lmsscrobbling PRIVATE
	lmscore
	)

target_link_libraries(lmsscrobbling PUBLIC
	lmsdatabase
	)

if(BUILD_TESTING)
	add_subdirectory(test)
endif()
