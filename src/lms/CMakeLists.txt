
add_executable(lms
	main.cpp
	ui/Auth.cpp
	ui/Bootstrap5Theme.cpp
	ui/LmsApplication.cpp
	ui/LmsApplicationManager.cpp
	ui/LmsTheme.cpp
	ui/MediaPlayer.cpp
	ui/ModalManager.cpp
	ui/NotificationContainer.cpp
	ui/PlayQueue.cpp
	ui/SettingsView.cpp
	ui/TrackStringUtils.cpp
	ui/admin/DatabaseSettingsView.cpp
	ui/admin/ScannerController.cpp
	ui/admin/InitWizardView.cpp
	ui/admin/UserView.cpp
	ui/admin/UsersView.cpp
	ui/common/DirectoryValidator.cpp
	ui/common/InfiniteScrollingContainer.cpp
	ui/common/LoadingIndicator.cpp
	ui/common/LoginNameValidator.cpp
	ui/common/MandatoryValidator.cpp
	ui/common/PasswordValidator.cpp
	ui/common/UUIDValidator.cpp
	ui/explore/ArtistCollector.cpp
	ui/explore/ArtistListHelpers.cpp
	ui/explore/ArtistView.cpp
	ui/explore/ArtistsView.cpp
	ui/explore/DatabaseCollectorBase.cpp
	ui/explore/Explore.cpp
	ui/explore/Filters.cpp
	ui/explore/ReleaseCollector.cpp
	ui/explore/ReleaseListHelpers.cpp
	ui/explore/ReleasePopup.cpp
	ui/explore/ReleasesView.cpp
	ui/explore/ReleaseView.cpp
	ui/explore/SearchView.cpp
	ui/explore/TrackCollector.cpp
	ui/explore/TrackListHelpers.cpp
	ui/explore/TrackPopup.cpp
	ui/explore/TracksView.cpp
	ui/resource/AudioFileResource.cpp
	ui/resource/AudioTranscodeResource.cpp
	ui/resource/CoverResource.cpp
	ui/resource/DownloadResource.cpp
	)

target_include_directories(lms PRIVATE
	ui/
	)

target_link_libraries(lms PRIVATE
	lmsav
	lmsauth
	lmsdatabase
	lmsrecommendation
	lmsscanner
	lmsscrobbling
	lmsservice-cover
	lmssubsonic
	lmsutils
	Wt::Wt
	Wt::HTTP
	)

install(TARGETS lms DESTINATION bin)

