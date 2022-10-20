
add_executable(test-scrobbling
	Listenbrainz.cpp
	Scrobbling.cpp
	)

target_link_libraries(test-scrobbling PRIVATE
	lmsutils
	lmsscrobbling
	GTest::GTest
	)

target_include_directories(test-scrobbling PRIVATE
	../impl
	)

if (NOT CMAKE_CROSSCOMPILING)
	gtest_discover_tests(test-scrobbling)
endif()

