
AUTOMAKE_OPTIONS = dist-bzip2 no-dist-gzip

SUBDIRS = test .

bin_PROGRAMS = lms

lms_SOURCES = \
	$(top_srcdir)/main/main.cpp		\
	$(top_srcdir)/main/ServiceManager.cpp	\
	$(top_srcdir)/main/DatabaseRefreshService.cpp	\
	$(top_srcdir)/main/RemoteServerService.cpp	\
	$(top_srcdir)/main/WebServerService.cpp	\
	$(top_srcdir)/av/CodecContext.cpp	\
	$(top_srcdir)/av/Common.cpp		\
	$(top_srcdir)/av/Dictionary.cpp		\
	$(top_srcdir)/av/FormatContext.cpp	\
	$(top_srcdir)/av/InputFormatContext.cpp	\
	$(top_srcdir)/av/Stream.cpp		\
	$(top_srcdir)/ui/LmsApplication.cpp	\
	$(top_srcdir)/ui/audio/AudioWidget.cpp			\
	$(top_srcdir)/ui/audio/AudioDatabaseWidget.cpp		\
	$(top_srcdir)/ui/audio/AudioMediaPlayerWidget.cpp	\
	$(top_srcdir)/ui/audio/SearchFilterWidget.cpp		\
	$(top_srcdir)/ui/audio/TableFilterWidget.cpp		\
	$(top_srcdir)/ui/audio/TrackWidget.cpp			\
	$(top_srcdir)/ui/resource/AvConvTranscodeStreamResource.cpp	\
	$(top_srcdir)/ui/video/VideoWidget.cpp			\
	$(top_srcdir)/ui/video/VideoDatabaseWidget.cpp		\
	$(top_srcdir)/ui/video/VideoMediaPlayerWidget.cpp	\
	$(top_srcdir)/ui/video/VideoParametersDialog.cpp	\
	$(top_srcdir)/database/Artist.cpp		\
	$(top_srcdir)/database/Genre.cpp		\
	$(top_srcdir)/database/Release.cpp		\
	$(top_srcdir)/database/Track.cpp		\
	$(top_srcdir)/database/Database.cpp		\
	$(top_srcdir)/database/DatabaseHandler.cpp	\
	$(top_srcdir)/database/Checksum.cpp		\
	$(top_srcdir)/database/SqlQuery.cpp		\
	$(top_srcdir)/database/Path.cpp			\
	$(top_srcdir)/database/Video.cpp		\
	$(top_srcdir)/remote/server/Connection.cpp	\
	$(top_srcdir)/remote/server/ConnectionManager.cpp		\
	$(top_srcdir)/remote/server/Server.cpp		\
	$(top_srcdir)/transcode/AvConvTranscoder.cpp	\
	$(top_srcdir)/transcode/Format.cpp		\
	$(top_srcdir)/transcode/Parameters.cpp		\
	$(top_srcdir)/transcode/InputMediaFile.cpp	\
	$(top_srcdir)/metadata/AvFormat.cpp		\
	$(top_srcdir)/metadata/Extractor.cpp		\
	$(top_srcdir)/metadata/Utils.cpp

lms_CXXFLAGS=-std=c++11 -Wall -I$(top_srcdir)/boost/ -I$(top_srcdir)/ui -I$(top_srcdir)/remote

