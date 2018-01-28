bin_PROGRAMS = lms

lms_SOURCES = \
	$(srcdir)/main/main.cpp					\
	$(srcdir)/av/AvInfo.cpp					\
	$(srcdir)/av/AvTranscoder.cpp				\
	$(srcdir)/cover/CoverArtGrabber.cpp			\
	$(srcdir)/database/DbArtist.cpp				\
	$(srcdir)/database/DatabaseHandler.cpp			\
	$(srcdir)/database/MediaDirectory.cpp			\
	$(srcdir)/database/Playlist.cpp				\
	$(srcdir)/database/Release.cpp				\
	$(srcdir)/database/SearchFilter.cpp			\
	$(srcdir)/database/Setting.cpp				\
	$(srcdir)/database/SqlQuery.cpp				\
	$(srcdir)/database/Track.cpp				\
	$(srcdir)/database/User.cpp				\
	$(srcdir)/database/Video.cpp				\
	$(srcdir)/database/updater/DatabaseUpdater.cpp		\
	$(srcdir)/database/updater/DatabaseFeatureExtractor.cpp	\
	$(srcdir)/database/updater/DatabaseHighLevelCluster.cpp	\
	$(srcdir)/feature/FeatureExtractor.cpp			\
	$(srcdir)/feature/FeatureStore.cpp			\
	$(srcdir)/image/Image.cpp				\
	$(srcdir)/metadata/AvFormat.cpp				\
	$(srcdir)/metadata/TagLibParser.cpp			\
	$(srcdir)/ui/ArtistView.cpp				\
	$(srcdir)/ui/ArtistsView.cpp				\
	$(srcdir)/ui/Explore.cpp				\
	$(srcdir)/ui/Filters.cpp				\
	$(srcdir)/ui/HomeView.cpp				\
	$(srcdir)/ui/LmsApplication.cpp				\
	$(srcdir)/ui/ReleasesView.cpp				\
	$(srcdir)/ui/ReleaseView.cpp				\
	$(srcdir)/ui/TracksView.cpp				\
	$(srcdir)/ui/auth/LmsAuth.cpp				\
	$(srcdir)/ui/common/DirectoryValidator.cpp		\
	$(srcdir)/ui/common/InputRange.cpp			\
	$(srcdir)/ui/common/LineEdit.cpp			\
	$(srcdir)/ui/resource/AvConvTranscodeStreamResource.cpp	\
	$(srcdir)/ui/resource/ImageResource.cpp			\
	$(srcdir)/ui/resource/TranscodeResource.cpp		\
	$(srcdir)/utils/Config.cpp				\
	$(srcdir)/utils/Logger.cpp				\
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

