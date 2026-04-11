
add_executable(bench-som
	Matrix.cpp
	Network.cpp
	SomBench.cpp
	)

target_link_libraries(bench-som PRIVATE
	lmssom
	benchmark
	)
