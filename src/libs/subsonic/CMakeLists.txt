
add_library(lmssubsonic SHARED
	impl/responses/Album.cpp
	impl/responses/Artist.cpp
	impl/ProtocolVersion.cpp
	impl/Scan.cpp
	impl/Stream.cpp
	impl/SubsonicId.cpp
	impl/SubsonicResource.cpp
	impl/SubsonicResponse.cpp
	)

target_include_directories(lmssubsonic INTERFACE
	include
	)

target_include_directories(lmssubsonic PRIVATE
	impl
	include
	)

target_link_libraries(lmssubsonic PRIVATE
	lmsauth
	lmsav
	lmsdatabase
	lmsrecommendation
	lmsscanner
	lmsscrobbling
	lmsservice-cover
	lmsutils
	std::filesystem
	)

target_link_libraries(lmssubsonic PUBLIC
	Wt::Wt
	)

install(TARGETS lmssubsonic DESTINATION lib)

