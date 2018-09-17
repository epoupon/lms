bin_PROGRAMS = lms

lms_SOURCES = \
	$(srcdir)/main/main.cpp					\
	$(srcdir)/av/AvInfo.cpp					\
	$(srcdir)/av/AvTranscoder.cpp				\
	$(srcdir)/cover/CoverArtGrabber.cpp			\
	$(srcdir)/database/Artist.cpp				\
	$(srcdir)/database/Cluster.cpp				\
	$(srcdir)/database/DatabaseHandler.cpp			\
	$(srcdir)/database/TrackList.cpp				\
	$(srcdir)/database/Release.cpp				\
	$(srcdir)/database/ScanSettings.cpp			\
	$(srcdir)/database/SqlQuery.cpp				\
	$(srcdir)/database/Track.cpp				\
	$(srcdir)/database/User.cpp				\
	$(srcdir)/image/Image.cpp				\
	$(srcdir)/metadata/AvFormat.cpp				\
	$(srcdir)/metadata/TagLibParser.cpp			\
	$(srcdir)/scanner/MediaScanner.cpp			\
	$(srcdir)/ui/Auth.cpp					\
	$(srcdir)/ui/LmsApplication.cpp				\
	$(srcdir)/ui/LmsApplicationGroup.cpp			\
	$(srcdir)/ui/MediaPlayer.cpp				\
	$(srcdir)/ui/PlayQueueView.cpp				\
	$(srcdir)/ui/PlayHistoryView.cpp			\
	$(srcdir)/ui/SettingsView.cpp				\
	$(srcdir)/ui/admin/DatabaseSettingsView.cpp		\
	$(srcdir)/ui/admin/InitWizardView.cpp			\
	$(srcdir)/ui/admin/UserView.cpp				\
	$(srcdir)/ui/admin/UsersView.cpp			\
	$(srcdir)/ui/common/Validators.cpp			\
	$(srcdir)/ui/explore/ArtistView.cpp			\
	$(srcdir)/ui/explore/ArtistsInfoView.cpp		\
	$(srcdir)/ui/explore/ArtistsView.cpp			\
	$(srcdir)/ui/explore/Explore.cpp			\
	$(srcdir)/ui/explore/Filters.cpp			\
	$(srcdir)/ui/explore/ReleasesInfoView.cpp		\
	$(srcdir)/ui/explore/ReleasesView.cpp			\
	$(srcdir)/ui/explore/ReleaseView.cpp			\
	$(srcdir)/ui/explore/TracksInfoView.cpp			\
	$(srcdir)/ui/explore/TracksView.cpp			\
	$(srcdir)/ui/resource/ImageResource.cpp			\
	$(srcdir)/ui/resource/AudioResource.cpp			\
	$(srcdir)/utils/Config.cpp				\
	$(srcdir)/utils/Logger.cpp				\
	$(srcdir)/utils/Path.cpp				\
	$(srcdir)/utils/Utils.cpp

lms_CXXFLAGS=-std=c++14 -Wall -I$(srcdir)/ui $(MAGICKXX_CFLAGS) -D_REENTRANT
lms_LDADD=$(MAGICKXX_LIBS)

