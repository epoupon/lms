include(GoogleTest)

add_executable(test-utils
	String.cpp
	RecursiveSharedMutex.cpp
	Utils.cpp
	)

target_link_libraries(test-utils PRIVATE
	lmsutils
	Threads::Threads
	GTest::GTest
	)

gtest_discover_tests(test-utils)

