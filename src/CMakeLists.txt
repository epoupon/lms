add_compile_options(-Wall -Wextra -pedantic)

add_subdirectory(libs)
add_subdirectory(lms)
add_subdirectory(tools)

if(BUILD_TESTING)
	add_subdirectory(test)
endif()


