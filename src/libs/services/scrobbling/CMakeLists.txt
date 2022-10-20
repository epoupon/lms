
add_library(lmsscrobbling SHARED
	impl/internal/InternalScrobbler.cpp
	impl/listenbrainz/FeedbacksParser.cpp
	impl/listenbrainz/FeedbacksSynchronizer.cpp
	impl/listenbrainz/FeedbackTypes.cpp
	impl/listenbrainz/ListenBrainzScrobbler.cpp
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
	lmsutils
	)

target_link_libraries(lmsscrobbling PUBLIC
	lmsdatabase
	)

install(TARGETS lmsscrobbling DESTINATION lib)

if(BUILD_TESTING)
	add_subdirectory(test)
endif()
