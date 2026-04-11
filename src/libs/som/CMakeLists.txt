add_library(lmssom INTERFACE
	)

target_include_directories(lmssom INTERFACE
	include
	)

target_link_libraries(lmssom INTERFACE
	lmscore
	)

if(BUILD_TESTING)
	add_subdirectory(test)
endif()

if (BUILD_BENCHMARKS)
	add_subdirectory(bench)
endif()
