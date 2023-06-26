
add_library(lmsauth SHARED
	impl/AuthTokenService.cpp
	impl/AuthServiceBase.cpp
	impl/EnvService.cpp
	impl/LoginThrottler.cpp
	impl/PasswordServiceBase.cpp
	impl/http-headers/HttpHeadersEnvService.cpp
	impl/internal/InternalPasswordService.cpp
	)

target_include_directories(lmsauth INTERFACE
	include
	)

target_include_directories(lmsauth PRIVATE
	include
	impl
	)

target_link_libraries(lmsauth PRIVATE
	lmsutils
	lmsdatabase
	)

target_link_libraries(lmsauth PUBLIC
	Boost::system
	Wt::Wt
	)

if (USE_PAM)
	target_compile_options(lmsauth PRIVATE "-DLMS_SUPPORT_PAM")
	target_sources(lmsauth PRIVATE impl/pam/PAMPasswordService.cpp)
	target_include_directories(lmsauth PRIVATE  ${PAM_INCLUDE_DIR})
	target_link_libraries(lmsauth PRIVATE ${PAM_LIBRARIES})
endif (USE_PAM)

install(TARGETS lmsauth DESTINATION lib)

