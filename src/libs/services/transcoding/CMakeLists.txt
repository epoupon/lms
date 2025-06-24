add_library(lmstranscoding STATIC
	impl/TranscodingResourceHandler.cpp
	impl/TranscodingService.cpp
	)

target_include_directories(lmstranscoding INTERFACE
	include
	)

target_include_directories(lmstranscoding PRIVATE
	include
	impl
	)

target_link_libraries(lmstranscoding PRIVATE
	lmsav
	)

target_link_libraries(lmstranscoding PUBLIC
	lmsdatabase
	)

