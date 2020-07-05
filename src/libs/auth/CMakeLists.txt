
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

if (PAM_FOUND)
	target_compile_options(lmsauth PRIVATE "-DLMS_SUPPORT_PAM")
	target_sources(lmsauth PRIVATE impl/pam/PAM.cpp)
endif(PAM_FOUND)

install(TARGETS lmsauth DESTINATION lib)

