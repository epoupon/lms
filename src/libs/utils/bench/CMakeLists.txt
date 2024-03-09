
add_executable(bench-utils
	ProfilerBench.cpp
	)

target_link_libraries(bench-utils PRIVATE
	lmsutils
	benchmark
	)
