
add_library(lmsscrobbling SHARED
	impl/internal/InternalScrobbler.cpp
	impl/listenbrainz/ListenBrainzScrobbler.cpp
	impl/listenbrainz/ListensSynchronizer.cpp
	impl/listenbrainz/SendQueue.cpp
	impl/listenbrainz/Utils.cpp
	impl/Scrobbling.cpp
	)

target_include_directories(lmsscrobbling INTERFACE
	include
	)

target_include_directories(lmsscrobbling PRIVATE
	include
	impl
	)

target_link_libraries(lmsscrobbling PRIVATE
	lmsutils
	)

target_link_libraries(lmsscrobbling PUBLIC
	lmsdatabase
	)

install(TARGETS lmsscrobbling DESTINATION lib)

