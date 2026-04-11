include(GoogleTest)

add_executable(test-som
	Matrix.cpp
	Network.cpp
	Vector.cpp
	Som.cpp
	)

target_link_libraries(test-som PRIVATE
	lmssom
	GTest::GTest
	)

if (NOT CMAKE_CROSSCOMPILING)
	gtest_discover_tests(test-som)
endif()

