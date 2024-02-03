
add_executable(bench-som
	SomBench.cpp
	)

target_link_libraries(bench-som PRIVATE
	lmssom
	benchmark
	)
