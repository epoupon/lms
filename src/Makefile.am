bin_PROGRAMS = lms

lms_SOURCES = \
	$(srcdir)/main/main.cpp					\
	$(srcdir)/av/CodecContext.cpp				\
	$(srcdir)/av/Common.cpp					\
	$(srcdir)/av/Dictionary.cpp				\
	$(srcdir)/av/FormatContext.cpp				\
	$(srcdir)/av/InputFormatContext.cpp			\
	$(srcdir)/av/Stream.cpp					\
	$(srcdir)/config/ConfigReader.cpp			\
	$(srcdir)/cover/CoverArt.cpp				\
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
	$(srcdir)/logger/Logger.cpp				\
	$(srcdir)/metadata/AvFormat.cpp				\
	$(srcdir)/metadata/Utils.cpp				\
	$(srcdir)/service/ServiceManager.cpp			\
	$(srcdir)/service/DatabaseUpdateService.cpp		\
	$(srcdir)/transcode/AvConvTranscoder.cpp		\
	$(srcdir)/transcode/Format.cpp				\
	$(srcdir)/transcode/Parameters.cpp			\
	$(srcdir)/transcode/InputMediaFile.cpp			\
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
	$(srcdir)/ui/video/VideoWidget.cpp			\
	$(srcdir)/ui/video/VideoDatabaseWidget.cpp		\
	$(srcdir)/ui/video/VideoMediaPlayerWidget.cpp		\
	$(srcdir)/ui/video/VideoParametersDialog.cpp		\
	$(srcdir)/ui/settings/Settings.cpp			\
	$(srcdir)/ui/settings/SettingsAccountFormView.cpp	\
	$(srcdir)/ui/settings/SettingsAudioFormView.cpp		\
	$(srcdir)/ui/settings/SettingsDatabaseFormView.cpp	\
	$(srcdir)/ui/settings/SettingsFirstConnectionFormView.cpp	\
	$(srcdir)/ui/settings/SettingsMediaDirectories.cpp	\
	$(srcdir)/ui/settings/SettingsMediaDirectoryFormView.cpp	\
	$(srcdir)/ui/settings/SettingsUserFormView.cpp	\
	$(srcdir)/ui/settings/SettingsUsers.cpp

if LMSAPI
lms_SOURCES += \
	$(srcdir)/lms-api/server/Connection.cpp			\
	$(srcdir)/lms-api/server/ConnectionManager.cpp		\
	$(srcdir)/lms-api/server/AudioCollectionRequestHandler.cpp		\
	$(srcdir)/lms-api/server/AuthRequestHandler.cpp		\
	$(srcdir)/lms-api/server/MediaRequestHandler.cpp		\
	$(srcdir)/lms-api/server/RequestHandler.cpp		\
	$(srcdir)/lms-api/server/Server.cpp			\
	$(srcdir)/service/LmsAPIServerService.cpp

nodist_lms_SOURCES = \
	$(builddir)/auth.pb.cc			\
	$(builddir)/auth.pb.h			\
	$(builddir)/collection.pb.cc		\
	$(builddir)/collection.pb.h		\
	$(builddir)/media.pb.cc			\
	$(builddir)/media.pb.h			\
	$(builddir)/messages.pb.cc		\
	$(builddir)/messages.pb.h

BUILT_SOURCES = \
	$(builddir)/auth.pb.cc			\
	$(builddir)/auth.pb.h			\
	$(builddir)/collection.pb.cc		\
	$(builddir)/collection.pb.h		\
	$(builddir)/media.pb.cc			\
	$(builddir)/media.pb.h			\
	$(builddir)/messages.pb.cc		\
	$(builddir)/messages.pb.h

MOSTLYCLEANFILES = \
	$(builddir)/auth.pb.cc			\
	$(builddir)/auth.pb.h			\
	$(builddir)/collection.pb.cc		\
	$(builddir)/collection.pb.h		\
	$(builddir)/media.pb.cc			\
	$(builddir)/media.pb.h			\
	$(builddir)/messages.pb.cc		\
	$(builddir)/messages.pb.h

%.pb.cc %.pb.h: $(top_srcdir)/lms-api/proto/%.proto
	$(PROTOC) --proto_path=$(top_srcdir)/lms-api/proto/ --cpp_out=$(builddir)/ $^

endif

lms_CXXFLAGS=-DBOOST_LOG_DYN_LINK -std=c++11 -Wall -I$(top_srcdir)/third-party -I$(srcdir)/ui -I$(srcdir)/lms-api

