
add_executable(lms-localplayer
	LmsLocalplayer.cpp
	)

target_link_libraries(lms-localplayer PRIVATE
	lmslocalplayer
	lmsdatabase
	lmsutils
	)

# install(TARGETS lms-localplayer DESTINATION bin)
