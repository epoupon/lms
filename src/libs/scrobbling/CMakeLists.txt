
add_library(lmsscrobbling SHARED
	impl/internal/InternalScrobbler.cpp
	impl/listenbrainz/ListenBrainzScrobbler.cpp
	impl/Scrobbling.cpp
	)

target_include_directories(lmsscrobbling INTERFACE
	include
	)

target_include_directories(lmsscrobbling PRIVATE
	include
	impl
	)

target_link_libraries(lmsscrobbling PUBLIC
	lmsdatabase
	lmsutils
	)

install(TARGETS lmsscrobbling DESTINATION lib)

