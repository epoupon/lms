
add_library(lmsfeedback STATIC
	impl/internal/InternalBackend.cpp
	impl/listenbrainz/FeedbacksParser.cpp
	impl/listenbrainz/FeedbacksSynchronizer.cpp
	impl/listenbrainz/FeedbackTypes.cpp
	impl/listenbrainz/ListenBrainzBackend.cpp
	impl/listenbrainz/Utils.cpp
	impl/FeedbackService.cpp
	)

target_include_directories(lmsfeedback INTERFACE
	include
	)

target_include_directories(lmsfeedback PRIVATE
	include
	impl
	)

target_link_libraries(lmsfeedback PRIVATE
	lmscore
	)

target_link_libraries(lmsfeedback PUBLIC
	lmsdatabase
	)
