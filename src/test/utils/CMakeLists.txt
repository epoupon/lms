
add_executable(test-utils
	UtilsTest.cpp
	)

target_link_libraries(test-utils PRIVATE
	lmsutils
	Threads::Threads
	)

add_test(NAME utils COMMAND test-utils)

