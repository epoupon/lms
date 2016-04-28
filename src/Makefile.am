bin_PROGRAMS = lms

lms_SOURCES = \
	$(srcdir)/main/main.cpp					\
	$(srcdir)/av/AvInfo.cpp					\
	$(srcdir)/av/AvTranscoder.cpp					\
	$(srcdir)/cover/CoverArtGrabber.cpp			\
	$(srcdir)/database/Artist.cpp				\
	$(srcdir)/database/Classification.cpp			\
	$(srcdir)/database/DatabaseClassifier.cpp		\
	$(srcdir)/database/DatabaseHandler.cpp			\
	$(srcdir)/database/DatabaseUpdater.cpp			\
	$(srcdir)/database/MediaDirectory.cpp			\
	$(srcdir)/database/Playlist.cpp				\
	$(srcdir)/database/Release.cpp				\
	$(srcdir)/database/SearchFilter.cpp			\
	$(srcdir)/database/SqlQuery.cpp				\
	$(srcdir)/database/Track.cpp				\
	$(srcdir)/database/User.cpp				\
	$(srcdir)/database/Video.cpp				\
	$(srcdir)/feature/FeatureExtractor.cpp			\
	$(srcdir)/image/Image.cpp				\
	$(srcdir)/logger/Logger.cpp				\
	$(srcdir)/metadata/AvFormat.cpp				\
	$(srcdir)/ui/LmsApplication.cpp				\
	$(srcdir)/ui/auth/LmsAuth.cpp				\
	$(srcdir)/ui/audio/AudioPlayer.cpp			\
	$(srcdir)/ui/audio/desktop/DesktopAudio.cpp		\
	$(srcdir)/ui/audio/desktop/FilterChain.cpp		\
	$(srcdir)/ui/audio/desktop/KeywordSearchFilter.cpp	\
	$(srcdir)/ui/audio/desktop/PlayQueue.cpp		\
	$(srcdir)/ui/audio/desktop/TableFilter.cpp		\
	$(srcdir)/ui/audio/desktop/TrackView.cpp		\
	$(srcdir)/ui/audio/mobile/ArtistSearch.cpp		\
	$(srcdir)/ui/audio/mobile/ArtistSearchView.cpp		\
	$(srcdir)/ui/audio/mobile/ArtistView.cpp		\
	$(srcdir)/ui/audio/mobile/MobileAudio.cpp		\
	$(srcdir)/ui/audio/mobile/MobilePlayQueue.cpp		\
	$(srcdir)/ui/audio/mobile/PreviewSearchView.cpp		\
	$(srcdir)/ui/audio/mobile/ReleaseSearch.cpp		\
	$(srcdir)/ui/audio/mobile/ReleaseSearchView.cpp		\
	$(srcdir)/ui/audio/mobile/ReleaseView.cpp		\
	$(srcdir)/ui/audio/mobile/TrackSearch.cpp		\
	$(srcdir)/ui/audio/mobile/TrackSearchView.cpp		\
	$(srcdir)/ui/common/DirectoryValidator.cpp		\
	$(srcdir)/ui/common/InputRange.cpp			\
	$(srcdir)/ui/common/LineEdit.cpp			\
	$(srcdir)/ui/resource/AvConvTranscodeStreamResource.cpp	\
	$(srcdir)/ui/resource/ImageResource.cpp			\
	$(srcdir)/ui/resource/TranscodeResource.cpp		\
	$(srcdir)/ui/settings/Settings.cpp			\
	$(srcdir)/ui/settings/SettingsAccountFormView.cpp	\
	$(srcdir)/ui/settings/SettingsAudioFormView.cpp		\
	$(srcdir)/ui/settings/SettingsDatabaseFormView.cpp	\
	$(srcdir)/ui/settings/SettingsFirstConnectionFormView.cpp	\
	$(srcdir)/ui/settings/SettingsMediaDirectories.cpp	\
	$(srcdir)/ui/settings/SettingsMediaDirectoryFormView.cpp	\
	$(srcdir)/ui/settings/SettingsUserFormView.cpp		\
	$(srcdir)/ui/settings/SettingsUsers.cpp			\
	$(srcdir)/utils/Path.cpp				\
	$(srcdir)/utils/Utils.cpp

if VIDEO
lms_SOURCES += \
	$(srcdir)/ui/video/VideoWidget.cpp			\
	$(srcdir)/ui/video/VideoDatabaseWidget.cpp		\
	$(srcdir)/ui/video/VideoMediaPlayerWidget.cpp		\
	$(srcdir)/ui/video/VideoParametersDialog.cpp
endif

lms_CXXFLAGS=-std=c++11 -Wall -I$(srcdir)/third-party -I$(srcdir)/ui $(MAGICKXX_CFLAGS) -D_REENTRANT -DBOOST_SPIRIT_THREADSAFE
lms_LDADD=$(MAGICKXX_LIBS)

