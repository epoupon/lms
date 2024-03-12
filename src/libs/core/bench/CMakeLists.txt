
add_executable(bench-core
	TraceLoggerBench.cpp
	)

target_link_libraries(bench-core PRIVATE
	lmscore
	benchmark
	)
