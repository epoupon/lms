
add_executable(test-database
	Artist.cpp
	Cluster.cpp
	DatabaseTest.cpp
	Track.cpp
	)

target_link_libraries(test-database PRIVATE
	lmsdatabase
	GTest::GTest
	)

gtest_discover_tests(test-database)

