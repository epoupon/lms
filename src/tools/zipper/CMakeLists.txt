
add_executable(lms-zipper
	LmsZipper.cpp
	)

target_link_libraries(lms-zipper PRIVATE
	lmsutils
	)

