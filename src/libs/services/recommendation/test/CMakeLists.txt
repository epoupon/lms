add_executable(test-recommendation
	ConstraintsTest.cpp
	)

target_link_libraries(test-recommendation PRIVATE
	lmsrecommendation
	GTest::GTest
	GTest::gtest_main
	)

target_include_directories(test-recommendation PRIVATE
	../impl
	)

if (NOT CMAKE_CROSSCOMPILING)
	gtest_discover_tests(test-recommendation)
endif()
