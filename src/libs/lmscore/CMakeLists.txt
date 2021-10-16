
add_library(lmscore SHARED
	impl/auth/AuthTokenService.cpp
	impl/auth/AuthServiceBase.cpp
	impl/auth/EnvService.cpp
	impl/auth/LoginThrottler.cpp
	impl/auth/PasswordServiceBase.cpp
	impl/auth/http-headers/HttpHeadersEnvService.cpp
	impl/auth/internal/InternalPasswordService.cpp
	impl/cover/CoverService.cpp
	)

target_include_directories(lmscore INTERFACE
	include
	)

target_include_directories(lmscore PRIVATE
	include
	impl
	)

target_link_libraries(lmscore PRIVATE
	lmsav
	)

target_link_libraries(lmscore PUBLIC
	lmsdatabase
	lmsimage
	lmsutils
	std::filesystem
	Wt::Wt
	)

if (USE_PAM)
	target_compile_options(lmscore PRIVATE "-DLMS_SUPPORT_PAM")
	target_sources(lmscore PRIVATE impl/auth/pam/PAMPasswordService.cpp)
	target_include_directories(lmscore PRIVATE  ${PAM_INCLUDE_DIR})
	target_link_libraries(lmscore PRIVATE ${PAM_LIBRARIES})
endif (USE_PAM)


install(TARGETS lmscore DESTINATION lib)

