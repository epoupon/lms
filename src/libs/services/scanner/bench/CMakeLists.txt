
add_executable(bench-scanner
	Lyrics.cpp
	Scanner.cpp
	TrackMetadataParser.cpp
	)

target_include_directories(bench-scanner PRIVATE
	../impl
	../test
	)

target_link_libraries(bench-scanner PRIVATE
	lmsscanner
	lmsaudio
	benchmark
	)
