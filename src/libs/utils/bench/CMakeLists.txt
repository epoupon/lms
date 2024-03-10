
add_executable(bench-utils
	TraceLoggerBench.cpp
	)

target_link_libraries(bench-utils PRIVATE
	lmsutils
	benchmark
	)
