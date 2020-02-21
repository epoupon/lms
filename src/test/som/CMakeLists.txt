
add_executable(test-som
	SomTest.cpp
	)

target_link_libraries(test-som PRIVATE
	lmssom
	)

add_test(NAME som COMMAND test-som)

