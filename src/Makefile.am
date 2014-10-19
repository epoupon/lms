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
	$(srcdir)/database/Genre.cpp				\
	$(srcdir)/database/Release.cpp				\
	$(srcdir)/database/MediaDirectory.cpp			\
	$(srcdir)/database/Track.cpp				\
	$(srcdir)/database/DatabaseHandler.cpp			\
	$(srcdir)/database/SqlQuery.cpp				\
	$(srcdir)/database/Video.cpp				\
	$(srcdir)/database/User.cpp				\
	$(srcdir)/database-updater/DatabaseUpdater.cpp		\
	$(srcdir)/database-updater/Checksum.cpp			\
	$(srcdir)/logger/Logger.cpp				\
	$(srcdir)/metadata/AvFormat.cpp				\
	$(srcdir)/metadata/Utils.cpp				\
	$(srcdir)/remote/server/Connection.cpp			\
	$(srcdir)/remote/server/ConnectionManager.cpp		\
	$(srcdir)/remote/server/AudioCollectionRequestHandler.cpp		\
	$(srcdir)/remote/server/AuthRequestHandler.cpp		\
	$(srcdir)/remote/server/MediaRequestHandler.cpp		\
	$(srcdir)/remote/server/RequestHandler.cpp		\
	$(srcdir)/remote/server/Server.cpp			\
	$(srcdir)/service/ServiceManager.cpp			\
	$(srcdir)/service/DatabaseUpdateService.cpp		\
	$(srcdir)/service/UserInterfaceService.cpp		\
	$(srcdir)/service/RemoteServerService.cpp		\
	$(srcdir)/transcode/AvConvTranscoder.cpp		\
	$(srcdir)/transcode/Format.cpp				\
	$(srcdir)/transcode/Parameters.cpp			\
	$(srcdir)/transcode/InputMediaFile.cpp			\
	$(srcdir)/ui/LmsApplication.cpp				\
	$(srcdir)/ui/auth/LmsAuth.cpp				\
	$(srcdir)/ui/audio/AudioWidget.cpp			\
	$(srcdir)/ui/audio/AudioMediaPlayerWidget.cpp		\
	$(srcdir)/ui/audio/FilterChain.cpp			\
	$(srcdir)/ui/audio/KeywordSearchFilter.cpp		\
	$(srcdir)/ui/audio/TableFilter.cpp			\
	$(srcdir)/ui/audio/TrackView.cpp			\
	$(srcdir)/ui/common/DirectoryValidator.cpp		\
	$(srcdir)/ui/common/SessionData.cpp			\
	$(srcdir)/ui/resource/AvConvTranscodeStreamResource.cpp	\
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

nodist_lms_SOURCES = \
	$(builddir)/auth.pb.cc			\
	$(builddir)/auth.pb.h			\
	$(builddir)/collection.pb.cc		\
	$(builddir)/collection.pb.h		\
	$(builddir)/common.pb.cc		\
	$(builddir)/common.pb.h			\
	$(builddir)/media.pb.cc			\
	$(builddir)/media.pb.h			\
	$(builddir)/messages.pb.cc		\
	$(builddir)/messages.pb.h

BUILT_SOURCES = \
	$(builddir)/auth.pb.cc			\
	$(builddir)/auth.pb.h			\
	$(builddir)/collection.pb.cc		\
	$(builddir)/collection.pb.h		\
	$(builddir)/common.pb.cc		\
	$(builddir)/common.pb.h			\
	$(builddir)/media.pb.cc			\
	$(builddir)/media.pb.h			\
	$(builddir)/messages.pb.cc		\
	$(builddir)/messages.pb.h

MOSTLYCLEANFILES = \
	$(builddir)/auth.pb.cc			\
	$(builddir)/auth.pb.h			\
	$(builddir)/collection.pb.cc		\
	$(builddir)/collection.pb.h		\
	$(builddir)/common.pb.cc		\
	$(builddir)/common.pb.h			\
	$(builddir)/media.pb.cc			\
	$(builddir)/media.pb.h			\
	$(builddir)/messages.pb.cc		\
	$(builddir)/messages.pb.h

%.pb.cc %.pb.h: $(srcdir)/remote/proto/%.proto
	$(PROTOC) --proto_path=$(srcdir)/remote/proto/ --cpp_out=$(builddir)/ $^

lms_CXXFLAGS=-DBOOST_LOG_DYN_LINK -std=c++11 -Wall -I$(top_srcdir)/third-party -I$(srcdir)/ui -I$(srcdir)/remote

