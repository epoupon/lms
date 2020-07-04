
add_library(lmsauth SHARED
	impl/AuthTokenService.cpp
	impl/PasswordService.cpp
	impl/LoginThrottler.cpp
	)

target_include_directories(lmsauth INTERFACE
	include
	)

target_include_directories(lmsauth PRIVATE
	include/
	)

target_link_libraries(lmsauth PRIVATE
	lmsutils
	lmsdatabase
	)

target_link_libraries(lmsauth PUBLIC
	pthread
	boost_system
	wt
	${PAM_LIBRARIES}
	)

install(TARGETS lmsauth DESTINATION lib)

