
TESTS = database-integrity sql-query database-user remote

check_PROGRAMS = database-integrity sql-query database-user remote

remote_SOURCES = \
	$(srcdir)/RemoteClientServer.cpp			\
	$(srcdir)/TestDatabase.cpp				\
	$(top_srcdir)/logger/Logger.cpp 			\
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
	$(top_srcdir)/database/MediaDirectory.cpp		\
	$(top_srcdir)/database/SqlQuery.cpp		\
	$(top_srcdir)/database/User.cpp			\
	$(top_srcdir)/database/Video.cpp

remote_CXXFLAGS=-std=c++11 -Wall -Wextra -DBOOST_LOG_DYN_LINK -I$(top_srcdir) -I$(top_srcdir)/remote -I$(top_srcdir)/boost

database_user_SOURCES = \
	$(srcdir)/CheckDatabaseUser.cpp		\
	$(top_srcdir)/logger/Logger.cpp 			\
	$(top_srcdir)/database/Artist.cpp	\
	$(top_srcdir)/database/Genre.cpp	\
	$(top_srcdir)/database/Release.cpp	\
	$(top_srcdir)/database/Track.cpp	\
	$(top_srcdir)/database/DatabaseHandler.cpp	\
	$(top_srcdir)/database/MediaDirectory.cpp		\
	$(top_srcdir)/database/SqlQuery.cpp		\
	$(top_srcdir)/database/User.cpp			\
	$(top_srcdir)/database/Video.cpp

database_user_CXXFLAGS=-std=c++11 -Wall -Wextra -DBOOST_LOG_DYN_LINK -I$(top_srcdir)


database_integrity_SOURCES = \
	$(srcdir)/DatabaseIntegrity.cpp		\
	$(top_srcdir)/logger/Logger.cpp 			\
	$(top_srcdir)/database/Artist.cpp	\
	$(top_srcdir)/database/Genre.cpp	\
	$(top_srcdir)/database/Release.cpp	\
	$(top_srcdir)/database/Track.cpp	\
	$(top_srcdir)/database/DatabaseHandler.cpp	\
	$(top_srcdir)/database/MediaDirectory.cpp		\
	$(top_srcdir)/database/SqlQuery.cpp		\
	$(top_srcdir)/database/User.cpp			\
	$(top_srcdir)/database/Video.cpp

database_integrity_CXXFLAGS=-std=c++11 -Wall -Wextra -DBOOST_LOG_DYN_LINK -I$(top_srcdir)

sql_query_SOURCES = \
	$(srcdir)/CheckSqlQuery.cpp         \
	$(top_srcdir)/database/SqlQuery.cpp

sql_query_CXXFLAGS=-std=c++11 -Wall -Wextra -I$(top_srcdir)

