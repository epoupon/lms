
add_executable(lms
	main.cpp
	ui/Auth.cpp
	ui/LmsApplication.cpp
	ui/LmsApplicationGroup.cpp
	ui/MediaPlayer.cpp
	ui/PlayQueueView.cpp
	ui/PlayHistoryView.cpp
	ui/SettingsView.cpp
	ui/TrackStringUtils.cpp
	ui/admin/DatabaseSettingsView.cpp
	ui/admin/DatabaseStatus.cpp
	ui/admin/InitWizardView.cpp
	ui/admin/UserView.cpp
	ui/admin/UsersView.cpp
	ui/common/Validators.cpp
	ui/explore/ArtistInfoView.cpp
	ui/explore/ArtistLink.cpp
	ui/explore/ArtistsInfoView.cpp
	ui/explore/ArtistView.cpp
	ui/explore/ArtistsView.cpp
	ui/explore/Explore.cpp
	ui/explore/Filters.cpp
	ui/explore/ReleaseInfoView.cpp
	ui/explore/ReleaseLink.cpp
	ui/explore/ReleasesInfoView.cpp
	ui/explore/ReleasesView.cpp
	ui/explore/ReleaseView.cpp
	ui/explore/TracksInfoView.cpp
	ui/explore/TracksView.cpp
	ui/resource/ImageResource.cpp
	ui/resource/AudioResource.cpp
	)

target_include_directories(lms PRIVATE
	ui/
	)

target_link_libraries(lms PRIVATE
	lmsav
	lmsauth
	lmscover
	lmsdatabase
	lmsrecommendation
	lmsscanner
	lmssubsonic
	lmsutils
	wt
	wthttp
	)

install(TARGETS lms DESTINATION bin)

