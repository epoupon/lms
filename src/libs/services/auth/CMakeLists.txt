
add_library(lmsauth STATIC
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

target_link_libraries(lmsauth PUBLIC
	Boost::system
	Wt::Wt
	lmscore
	lmsdatabase
	)
	
# PAM
option(USE_PAM "Use the PAM backend authentication API" ON)
if (USE_PAM)
	find_package(PAM QUIET)
	if (USE_PAM AND NOT PAM_FOUND)
		message(WARNING "PAM library not found: disabling")
		set(USE_PAM OFF)
	endif ()
endif ()

if (USE_PAM)
	message(STATUS "Using PAM authentication backend")
else ()
	message(STATUS "NOT using PAM authentication backend")
endif ()

if (USE_PAM)
	target_compile_options(lmsauth PRIVATE "-DLMS_SUPPORT_PAM")
	target_sources(lmsauth PRIVATE impl/pam/PAMPasswordService.cpp)
	target_include_directories(lmsauth PRIVATE  ${PAM_INCLUDE_DIR})
	target_link_libraries(lmsauth PRIVATE ${PAM_LIBRARIES})
endif (USE_PAM)
