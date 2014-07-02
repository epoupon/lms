
TESTS = database-integrity sql-query database-basics remote

check_PROGRAMS = database-integrity sql-query database-basics remote

remote_SOURCES = \
	$(srcdir)/RemoteClientServer.cpp			\
	$(srcdir)/TestDatabase.cpp				\
	$(top_srcdir)/remote/messages/auth.pb.cc		\
	$(top_srcdir)/remote/messages/collection.pb.cc		\
	$(top_srcdir)/remote/messages/common.pb.cc		\
	$(top_srcdir)/remote/messages/media.pb.cc		\
	$(top_srcdir)/remote/messages/messages.pb.cc		\
	$(top_srcdir)/database/Artist.cpp			\
	$(top_srcdir)/database/Genre.cpp			\
	$(top_srcdir)/database/Release.cpp			\
	$(top_srcdir)/database/Track.cpp			\
	$(top_srcdir)/database/DatabaseHandler.cpp		\
	$(top_srcdir)/database/SqlQuery.cpp		\
	$(top_srcdir)/database/Path.cpp				\
	$(top_srcdir)/database/Video.cpp

remote_CXXFLAGS=-std=c++11 -Wall -Wextra -I$(top_srcdir) -I$(top_srcdir)/remote -I$(top_srcdir)/boost

database_integrity_SOURCES = \
	$(srcdir)/DatabaseIntegrity.cpp		\
	$(top_srcdir)/database/Artist.cpp	\
	$(top_srcdir)/database/Genre.cpp	\
	$(top_srcdir)/database/Release.cpp	\
	$(top_srcdir)/database/Track.cpp	\
	$(top_srcdir)/database/DatabaseHandler.cpp	\
	$(top_srcdir)/database/SqlQuery.cpp		\
	$(top_srcdir)/database/Path.cpp			\
	$(top_srcdir)/database/Video.cpp

database_integrity_CXXFLAGS=-std=c++11 -Wall -Wextra -I$(top_srcdir)

database_basics_SOURCES = \
	$(srcdir)/CheckDatabaseBasics.cpp		\
	$(top_srcdir)/database/Artist.cpp	\
	$(top_srcdir)/database/Genre.cpp	\
	$(top_srcdir)/database/Release.cpp	\
	$(top_srcdir)/database/Track.cpp	\
	$(top_srcdir)/database/DatabaseHandler.cpp	\
	$(top_srcdir)/database/MediaDirectory.cpp	\
	$(top_srcdir)/database/SqlQuery.cpp		\
	$(top_srcdir)/database/Path.cpp			\
	$(top_srcdir)/database/Video.cpp

database_basics_CXXFLAGS=-std=c++11 -Wall -Wextra -I$(top_srcdir)


sql_query_SOURCES = \
	$(srcdir)/CheckSqlQuery.cpp         \
	$(top_srcdir)/database/SqlQuery.cpp

sql_query_CXXFLAGS=-std=c++11 -Wall -Wextra -I$(top_srcdir)

