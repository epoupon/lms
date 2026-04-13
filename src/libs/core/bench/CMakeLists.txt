
add_executable(bench-core
	math/EuclideanDistance.cpp
	Core.cpp
	TraceLoggerBench.cpp
	)

target_link_libraries(bench-core PRIVATE
	lmscore
	benchmark
	)
