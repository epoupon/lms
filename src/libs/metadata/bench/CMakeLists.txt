
add_executable(bench-metadata
	LyricsBench.cpp
	Metadata.cpp
	)

target_include_directories(bench-metadata PRIVATE
	../impl
	../test
	)

target_link_libraries(bench-metadata PRIVATE
	lmsmetadata
	benchmark
	)
