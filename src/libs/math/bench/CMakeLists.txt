
add_executable(bench-math
	FFT.cpp
	EuclideanDistance.cpp
	Math.cpp
	)

target_link_libraries(bench-math PRIVATE
	lmscore
	lmsmath
	benchmark
	)

target_compile_options(bench-math PRIVATE
	$<$<NOT:$<CONFIG:Debug>>:-ffast-math>
	)
