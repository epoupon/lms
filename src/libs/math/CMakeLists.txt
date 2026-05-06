add_library(lmsmath INTERFACE)

target_include_directories(lmsmath INTERFACE
	include
)

if(BUILD_TESTING)
	add_subdirectory(test)
endif()

if (BUILD_BENCHMARKS)
	add_subdirectory(bench)
endif()

