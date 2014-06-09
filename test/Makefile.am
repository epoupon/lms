
TESTS = database-integrity sql-query database-basics remote

check_PROGRAMS = database-integrity sql-query database-basics remote

remote_SOURCES = \
	$(srcdir)/RemoteClientServer.cpp			\
	$(srcdir)/TestDatabase.cpp				\
	$(top_srcdir)/av/CodecContext.cpp			\
	$(top_srcdir)/av/Common.cpp				\
	$(top_srcdir)/av/Dictionary.cpp				\
	$(top_srcdir)/av/FormatContext.cpp			\
	$(top_srcdir)/av/InputFormatContext.cpp			\
	$(top_srcdir)/av/Stream.cpp				\
	$(top_srcdir)/cover/CoverArtGrabber.cpp			\
	$(top_srcdir)/remote/server/Server.cpp			\
	$(top_srcdir)/remote/server/RequestHandler.cpp		\
	$(top_srcdir)/remote/server/AudioCollectionRequestHandler.cpp		\
	$(top_srcdir)/remote/server/MediaRequestHandler.cpp	\
	$(top_srcdir)/remote/server/ConnectionManager.cpp	\
	$(top_srcdir)/remote/server/Connection.cpp		\
	$(top_srcdir)/remote/messages/auth.pb.cc		\
	$(top_srcdir)/remote/messages/collection.pb.cc		\
	$(top_srcdir)/remote/messages/common.pb.cc		\
	$(top_srcdir)/remote/messages/media.pb.cc		\
	$(top_srcdir)/remote/messages/messages.pb.cc		\
	$(top_srcdir)/transcode/AvConvTranscoder.cpp		\
	$(top_srcdir)/transcode/Format.cpp			\
	$(top_srcdir)/transcode/Parameters.cpp			\
	$(top_srcdir)/transcode/InputMediaFile.cpp		\
	$(top_srcdir)/database/Artist.cpp			\
	$(top_srcdir)/database/Genre.cpp			\
	$(top_srcdir)/database/Release.cpp			\
	$(top_srcdir)/database/Track.cpp			\
	$(top_srcdir)/database/DatabaseHandler.cpp		\
	$(top_srcdir)/database/SqlQuery.cpp		\
	$(top_srcdir)/database/Path.cpp				\
	$(top_srcdir)/database/Video.cpp			\
	$(top_srcdir)/database/Checksum.cpp

remote_CXXFLAGS=-std=c++11 -Wall -Wextra -I$(top_srcdir) -I$(top_srcdir)/remote

database_integrity_SOURCES = \
	$(srcdir)/DatabaseIntegrity.cpp		\
	$(top_srcdir)/database/Artist.cpp	\
	$(top_srcdir)/database/Genre.cpp	\
	$(top_srcdir)/database/Release.cpp	\
	$(top_srcdir)/database/Track.cpp	\
	$(top_srcdir)/database/DatabaseHandler.cpp	\
	$(top_srcdir)/database/SqlQuery.cpp		\
	$(top_srcdir)/database/Path.cpp			\
	$(top_srcdir)/database/Video.cpp			\
	$(top_srcdir)/database/Checksum.cpp

database_integrity_CXXFLAGS=-std=c++11 -Wall -Wextra -I$(top_srcdir)

database_basics_SOURCES = \
	$(srcdir)/CheckDatabaseBasics.cpp		\
	$(top_srcdir)/database/Artist.cpp	\
	$(top_srcdir)/database/Genre.cpp	\
	$(top_srcdir)/database/Release.cpp	\
	$(top_srcdir)/database/Track.cpp	\
	$(top_srcdir)/database/DatabaseHandler.cpp	\
	$(top_srcdir)/database/SqlQuery.cpp		\
	$(top_srcdir)/database/Path.cpp			\
	$(top_srcdir)/database/Video.cpp			\
	$(top_srcdir)/database/Checksum.cpp

database_basics_CXXFLAGS=-std=c++11 -Wall -Wextra -I$(top_srcdir)


sql_query_SOURCES = \
	$(srcdir)/CheckSqlQuery.cpp         \
	$(top_srcdir)/database/SqlQuery.cpp

sql_query_CXXFLAGS=-std=c++11 -Wall -Wextra -I$(top_srcdir)

