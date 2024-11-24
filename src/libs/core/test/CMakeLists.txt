include(GoogleTest)

add_executable(test-core
	EnumSet.cpp
	LiteralString.cpp
	Path.cpp
	RecursiveSharedMutex.cpp
	Service.cpp
	String.cpp
	TraceLogger.cpp
	Utils.cpp
	UUID.cpp
	)

target_link_libraries(test-core PRIVATE
	lmscore
	Threads::Threads
	GTest::GTest
	)

if (NOT CMAKE_CROSSCOMPILING)
	gtest_discover_tests(test-core)
endif()

