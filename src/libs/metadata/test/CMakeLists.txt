include(GoogleTest)

add_executable(test-metadata
	Metadata.cpp
	Utils.cpp
	)

target_include_directories(test-metadata PRIVATE
	../impl
	)

target_link_libraries(test-metadata PRIVATE
	lmsmetadata
	GTest::GTest
	)

gtest_discover_tests(test-metadata)

