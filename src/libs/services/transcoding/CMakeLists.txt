add_library(lmstranscoding STATIC
	impl/TranscodeResourceHandler.cpp
	impl/TranscodeService.cpp
	)

target_include_directories(lmstranscoding INTERFACE
	include
	)

target_include_directories(lmstranscoding PRIVATE
	include
	impl
	)

target_link_libraries(lmstranscoding PRIVATE
	lmsaudio
	)

target_link_libraries(lmstranscoding PUBLIC
	lmsdatabase
	)

