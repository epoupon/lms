bin_PROGRAMS = lms

lms_SOURCES = \
	$(srcdir)/main/main.cpp					\
	$(srcdir)/av/AvInfo.cpp					\
	$(srcdir)/av/AvTranscoder.cpp					\
	$(srcdir)/cover/CoverArtGrabber.cpp			\
	$(srcdir)/database/Artist.cpp				\
	$(srcdir)/database/DatabaseHandler.cpp			\
	$(srcdir)/database/MediaDirectory.cpp			\
	$(srcdir)/database/Playlist.cpp				\
	$(srcdir)/database/Release.cpp				\
	$(srcdir)/database/SearchFilter.cpp			\
	$(srcdir)/database/SqlQuery.cpp				\
	$(srcdir)/database/Track.cpp				\
	$(srcdir)/database/User.cpp				\
	$(srcdir)/database/Video.cpp				\
	$(srcdir)/database-updater/DatabaseUpdater.cpp		\
	$(srcdir)/database-updater/Checksum.cpp			\
	$(srcdir)/image/Image.cpp				\
	$(srcdir)/logger/Logger.cpp				\
	$(srcdir)/metadata/AvFormat.cpp				\
	$(srcdir)/metadata/Utils.cpp				\
	$(srcdir)/service/ServiceManager.cpp			\
	$(srcdir)/service/DatabaseUpdateService.cpp		\
	$(srcdir)/ui/LmsApplication.cpp				\
	$(srcdir)/ui/auth/LmsAuth.cpp				\
	$(srcdir)/ui/audio/desktop/DesktopAudio.cpp		\
	$(srcdir)/ui/audio/desktop/AudioMediaPlayer.cpp		\
	$(srcdir)/ui/audio/desktop/FilterChain.cpp		\
	$(srcdir)/ui/audio/desktop/KeywordSearchFilter.cpp	\
	$(srcdir)/ui/audio/desktop/PlayQueue.cpp		\
	$(srcdir)/ui/audio/desktop/TableFilter.cpp		\
	$(srcdir)/ui/audio/desktop/TrackView.cpp		\
	$(srcdir)/ui/audio/mobile/ArtistSearch.cpp		\
	$(srcdir)/ui/audio/mobile/MobileAudio.cpp		\
	$(srcdir)/ui/audio/mobile/MobileAudioMediaPlayer.cpp		\
	$(srcdir)/ui/audio/mobile/ReleaseSearch.cpp		\
	$(srcdir)/ui/audio/mobile/TrackSearch.cpp		\
	$(srcdir)/ui/common/DirectoryValidator.cpp		\
	$(srcdir)/ui/common/LineEdit.cpp			\
	$(srcdir)/ui/resource/AvConvTranscodeStreamResource.cpp	\
	$(srcdir)/ui/resource/CoverResource.cpp			\
	$(srcdir)/ui/settings/Settings.cpp			\
	$(srcdir)/ui/settings/SettingsAccountFormView.cpp	\
	$(srcdir)/ui/settings/SettingsAudioFormView.cpp		\
	$(srcdir)/ui/settings/SettingsDatabaseFormView.cpp	\
	$(srcdir)/ui/settings/SettingsFirstConnectionFormView.cpp	\
	$(srcdir)/ui/settings/SettingsMediaDirectories.cpp	\
	$(srcdir)/ui/settings/SettingsMediaDirectoryFormView.cpp	\
	$(srcdir)/ui/settings/SettingsUserFormView.cpp	\
	$(srcdir)/ui/settings/SettingsUsers.cpp

if VIDEO
lms_SOURCES += \
	$(srcdir)/ui/video/VideoWidget.cpp			\
	$(srcdir)/ui/video/VideoDatabaseWidget.cpp		\
	$(srcdir)/ui/video/VideoMediaPlayerWidget.cpp		\
	$(srcdir)/ui/video/VideoParametersDialog.cpp
endif

lms_CXXFLAGS=-std=c++11 -Wall -I$(top_srcdir)/third-party -I$(srcdir)/ui $(MAGICKXX_CFLAGS)

lms_LDADD=$(MAGICKXX_LIBS)
