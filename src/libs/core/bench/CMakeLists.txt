
add_executable(bench-core
	Core.cpp
	TraceLoggerBench.cpp
	UUIDBench.cpp
	)

target_link_libraries(bench-core PRIVATE
	lmscore
	benchmark
	)
