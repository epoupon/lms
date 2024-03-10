include(GoogleTest)

add_executable(test-utils
	EnumSet.cpp
	LiteralString.cpp
	Path.cpp
	RecursiveSharedMutex.cpp
	String.cpp
	TraceLogger.cpp
	Utils.cpp
	)

target_link_libraries(test-utils PRIVATE
	lmsutils
	Threads::Threads
	GTest::GTest
	)

if (NOT CMAKE_CROSSCOMPILING)
	gtest_discover_tests(test-utils)
endif()

