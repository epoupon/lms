
add_executable(test-database
	DatabaseTest.cpp
	)

target_link_libraries(test-database PRIVATE
	lmsdatabase
	)

add_test(NAME database COMMAND test-database)

