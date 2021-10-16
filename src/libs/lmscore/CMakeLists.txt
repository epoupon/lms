
add_library(lmscore SHARED
	impl/auth/AuthTokenService.cpp
	impl/auth/AuthServiceBase.cpp
	impl/auth/EnvService.cpp
	impl/auth/LoginThrottler.cpp
	impl/auth/PasswordServiceBase.cpp
	impl/auth/http-headers/HttpHeadersEnvService.cpp
	impl/auth/internal/InternalPasswordService.cpp
	impl/database/Artist.cpp
	impl/database/Cluster.cpp
	impl/database/Db.cpp
	impl/database/TrackArtistLink.cpp
	impl/database/TrackFeatures.cpp
	impl/database/TrackList.cpp
	impl/database/Release.cpp
	impl/database/ScanSettings.cpp
	impl/database/Session.cpp
	impl/database/SqlQuery.cpp
	impl/database/Track.cpp
	impl/database/TrackBookmark.cpp
	impl/database/User.cpp
	impl/database/Utils.cpp
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
	Wt::DboSqlite3
	)

target_link_libraries(lmscore PUBLIC
	lmsdatabase
	lmsimage
	lmsutils
	std::filesystem
	Wt::Wt
	Wt::Dbo
	)

if (USE_PAM)
	target_compile_options(lmscore PRIVATE "-DLMS_SUPPORT_PAM")
	target_sources(lmscore PRIVATE impl/auth/pam/PAMPasswordService.cpp)
	target_include_directories(lmscore PRIVATE  ${PAM_INCLUDE_DIR})
	target_link_libraries(lmscore PRIVATE ${PAM_LIBRARIES})
endif (USE_PAM)


install(TARGETS lmscore DESTINATION lib)

if(BUILD_TESTING)
	add_subdirectory(test)
endif()
