include(GoogleTest)

add_executable(test-math
	ChamferDistance.cpp
	CentroidCalculator.cpp
	CosineDistance.cpp
	CovarianceCalculator.cpp
	DotProduct.cpp
	Entropy.cpp
	EuclideanDistance.cpp
	FFT.cpp
	MedoidCalculator.cpp
	NormalizedCosineDistance.cpp
	PrincipalComponents.cpp
	SquareMatrix.cpp
	StatsAccumulator.cpp
	Vector.cpp
	Window.cpp
)

target_include_directories(test-math PRIVATE
	../include
)

target_link_libraries(test-math PRIVATE
	lmscore
	lmsmath
	Threads::Threads
	GTest::GTest
	GTest::gtest_main
)

target_compile_options(test-math PRIVATE
	$<$<NOT:$<CONFIG:Debug>>:-ffast-math>
	)

if (NOT CMAKE_CROSSCOMPILING)
	gtest_discover_tests(test-math)
endif()
