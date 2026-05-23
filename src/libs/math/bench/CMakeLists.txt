
add_executable(bench-math
	ChamferDistance.cpp
	CosineDistance.cpp
	DotProduct.cpp
	EuclideanDistance.cpp
	FFT.cpp
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
