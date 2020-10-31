
add_library(lmsauth SHARED
	impl/AuthTokenService.cpp
	impl/PasswordService.cpp
	impl/LoginThrottler.cpp
	)

target_include_directories(lmsauth INTERFACE
	include
	)

target_include_directories(lmsauth PRIVATE
	include
	)

target_link_libraries(lmsauth PRIVATE
	lmsutils
	lmsdatabase
	)

target_link_libraries(lmsauth PUBLIC
	pthread
	Boost::system
	Wt::Wt
	)

if (USE_PAM)
	target_compile_options(lmsauth PRIVATE "-DLMS_SUPPORT_PAM")
	target_sources(lmsauth PRIVATE impl/pam/PAM.cpp)
	target_include_directories(lmsauth PRIVATE  ${PAM_INCLUDE_DIR})
	target_link_libraries(lmsauth PRIVATE ${PAM_LIBRARIES})
endif (USE_PAM)

install(TARGETS lmsauth DESTINATION lib)

