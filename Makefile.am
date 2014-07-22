
AUTOMAKE_OPTIONS = dist-bzip2 no-dist-gzip

SUBDIRS = test .

bin_PROGRAMS = lms

lms_SOURCES = \
	$(top_srcdir)/main/main.cpp			\
	$(top_srcdir)/av/CodecContext.cpp	\
	$(top_srcdir)/av/Common.cpp		\
	$(top_srcdir)/av/Dictionary.cpp		\
	$(top_srcdir)/av/FormatContext.cpp	\
	$(top_srcdir)/av/InputFormatContext.cpp	\
	$(top_srcdir)/av/Stream.cpp		\
	$(top_srcdir)/cover/CoverArt.cpp	\
	$(top_srcdir)/cover/CoverArtGrabber.cpp	\
	$(top_srcdir)/database/Artist.cpp		\
	$(top_srcdir)/database/Genre.cpp		\
	$(top_srcdir)/database/Release.cpp		\
	$(top_srcdir)/database/MediaDirectory.cpp	\
	$(top_srcdir)/database/Track.cpp		\
	$(top_srcdir)/database/DatabaseHandler.cpp	\
	$(top_srcdir)/database/SqlQuery.cpp		\
	$(top_srcdir)/database/Path.cpp			\
	$(top_srcdir)/database/Video.cpp		\
	$(top_srcdir)/database/User.cpp			\
	$(top_srcdir)/database-updater/DatabaseUpdater.cpp	\
	$(top_srcdir)/database-updater/Checksum.cpp		\
	$(top_srcdir)/metadata/AvFormat.cpp		\
	$(top_srcdir)/metadata/Utils.cpp		\
	$(top_srcdir)/remote/messages/auth.pb.cc		\
	$(top_srcdir)/remote/messages/collection.pb.cc		\
	$(top_srcdir)/remote/messages/common.pb.cc		\
	$(top_srcdir)/remote/messages/media.pb.cc		\
	$(top_srcdir)/remote/messages/messages.pb.cc		\
	$(top_srcdir)/remote/server/Connection.cpp		\
	$(top_srcdir)/remote/server/ConnectionManager.cpp	\
	$(top_srcdir)/remote/server/AudioCollectionRequestHandler.cpp		\
	$(top_srcdir)/remote/server/AuthRequestHandler.cpp	\
	$(top_srcdir)/remote/server/MediaRequestHandler.cpp	\
	$(top_srcdir)/remote/server/RequestHandler.cpp		\
	$(top_srcdir)/remote/server/Server.cpp		\
	$(top_srcdir)/service/ServiceManager.cpp		\
	$(top_srcdir)/service/DatabaseUpdateService.cpp	\
	$(top_srcdir)/service/UserInterfaceService.cpp		\
	$(top_srcdir)/service/RemoteServerService.cpp	\
	$(top_srcdir)/transcode/AvConvTranscoder.cpp	\
	$(top_srcdir)/transcode/Format.cpp		\
	$(top_srcdir)/transcode/Parameters.cpp		\
	$(top_srcdir)/transcode/InputMediaFile.cpp	\
	$(top_srcdir)/ui/LmsApplication.cpp	\
	$(top_srcdir)/ui/LmsHome.cpp		\
	$(top_srcdir)/ui/auth/LmsAuth.cpp			\
	$(top_srcdir)/ui/audio/AudioWidget.cpp			\
	$(top_srcdir)/ui/audio/AudioDatabaseWidget.cpp		\
	$(top_srcdir)/ui/audio/AudioMediaPlayerWidget.cpp	\
	$(top_srcdir)/ui/audio/SearchFilterWidget.cpp		\
	$(top_srcdir)/ui/audio/TableFilterWidget.cpp		\
	$(top_srcdir)/ui/audio/TrackWidget.cpp			\
	$(top_srcdir)/ui/common/DirectoryValidator.cpp		\
	$(top_srcdir)/ui/common/SessionData.cpp			\
	$(top_srcdir)/ui/resource/AvConvTranscodeStreamResource.cpp	\
	$(top_srcdir)/ui/video/VideoWidget.cpp			\
	$(top_srcdir)/ui/video/VideoDatabaseWidget.cpp		\
	$(top_srcdir)/ui/video/VideoMediaPlayerWidget.cpp	\
	$(top_srcdir)/ui/video/VideoParametersDialog.cpp	\
	$(top_srcdir)/ui/settings/Settings.cpp		\
	$(top_srcdir)/ui/settings/SettingsDatabase.cpp


lms_CXXFLAGS=-std=c++11 -Wall -I$(top_srcdir)/boost/ -I$(top_srcdir)/ui -I$(top_srcdir)/remote

