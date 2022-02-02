include(GoogleTest)

add_executable(test-som
	SomTest.cpp
	)

target_link_libraries(test-som PRIVATE
	lmssom
	GTest::GTest
	)

if (NOT CMAKE_CROSSCOMPILING)
	gtest_discover_tests(test-som)
endif()

