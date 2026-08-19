include(GoogleTest)

add_executable(test-image
	FormatSignature.cpp
	)

target_include_directories(test-image PRIVATE
	../impl
	)

target_link_libraries(test-image PRIVATE
	lmsimage
	GTest::GTest
	GTest::gtest_main
	)

if (NOT CMAKE_CROSSCOMPILING)
	gtest_discover_tests(test-image)
endif()
