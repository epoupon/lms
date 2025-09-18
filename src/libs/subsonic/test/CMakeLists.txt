include(GoogleTest)

add_executable(test-subsonic
	SubsonicResponseTest.cpp
	)

target_include_directories(test-subsonic PRIVATE
	../impl
	)

target_link_libraries(test-subsonic PRIVATE
	lmscore
	lmssubsonic
	GTest::GTest
	)

if (NOT CMAKE_CROSSCOMPILING)
	gtest_discover_tests(test-subsonic)
endif()

