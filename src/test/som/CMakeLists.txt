include(GoogleTest)

add_executable(test-som
	SomTest.cpp
	)

target_link_libraries(test-som PRIVATE
	lmssom
	GTest::GTest
	)

gtest_discover_tests(test-som)

