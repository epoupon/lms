
add_library(lmsfeedback SHARED
	impl/FeedbackService.cpp
	)

target_include_directories(lmsfeedback INTERFACE
	include
	)

target_include_directories(lmsfeedback PRIVATE
	include
	)

target_link_libraries(lmsfeedback PRIVATE
	lmsdatabase
	)

install(TARGETS lmsfeedback DESTINATION lib)

