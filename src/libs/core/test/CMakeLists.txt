include(GoogleTest)

add_executable(test-core
	EnumSet.cpp
	JobScheduler.cpp
	LiteralString.cpp
	PartialDateTime.cpp
	Path.cpp
	RecursiveSharedMutex.cpp
	Service.cpp
	String.cpp
	TraceLogger.cpp
	UUID.cpp
	XxHash3.cpp
	)

target_link_libraries(test-core PRIVATE
	lmscore
	lmsmath
	Threads::Threads
	GTest::GTest
	GTest::gtest_main
	)

if (NOT CMAKE_CROSSCOMPILING)
	gtest_discover_tests(test-core)
endif()

