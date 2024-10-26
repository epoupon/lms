
add_executable(bench-metadata
	LyricsBench.cpp
	)

target_link_libraries(bench-metadata PRIVATE
	lmsmetadata
	benchmark
	)
