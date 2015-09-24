
TESTS = database-basics database-integrity sql-query database-user

check_PROGRAMS = database-basics database-integrity sql-query database-user

database_basics_SOURCES = \
	$(srcdir)/CheckDbBasics.cpp			\
	$(top_srcdir)/src/logger/Logger.cpp 		\
	$(top_srcdir)/src/database/Artist.cpp		\
	$(top_srcdir)/src/database/DatabaseHandler.cpp	\
	$(top_srcdir)/src/database/MediaDirectory.cpp	\
	$(top_srcdir)/src/database/Playlist.cpp		\
	$(top_srcdir)/src/database/Release.cpp		\
	$(top_srcdir)/src/database/SearchFilter.cpp	\
	$(top_srcdir)/src/database/SqlQuery.cpp		\
	$(top_srcdir)/src/database/Track.cpp		\
	$(top_srcdir)/src/database/User.cpp		\
	$(top_srcdir)/src/database/Video.cpp

database_basics_CXXFLAGS=-std=c++11 -Wall -Wextra -DBOOST_LOG_DYN_LINK -I$(top_srcdir)/src

database_user_SOURCES = \
	$(srcdir)/CheckDatabaseUser.cpp		\
	$(top_srcdir)/src/logger/Logger.cpp 			\
	$(top_srcdir)/src/database/Artist.cpp		\
	$(top_srcdir)/src/database/Playlist.cpp	\
	$(top_srcdir)/src/database/Release.cpp		\
	$(top_srcdir)/src/database/Track.cpp	\
	$(top_srcdir)/src/database/DatabaseHandler.cpp	\
	$(top_srcdir)/src/database/MediaDirectory.cpp		\
	$(top_srcdir)/src/database/SearchFilter.cpp	\
	$(top_srcdir)/src/database/SqlQuery.cpp		\
	$(top_srcdir)/src/database/User.cpp			\
	$(top_srcdir)/src/database/Video.cpp

database_user_CXXFLAGS=-std=c++11 -Wall -Wextra -DBOOST_LOG_DYN_LINK -I$(top_srcdir)/src


database_integrity_SOURCES = \
	$(srcdir)/DatabaseIntegrity.cpp		\
	$(top_srcdir)/src/logger/Logger.cpp 			\
	$(top_srcdir)/src/database/Artist.cpp	\
	$(top_srcdir)/src/database/Playlist.cpp	\
	$(top_srcdir)/src/database/Track.cpp	\
	$(top_srcdir)/src/database/DatabaseHandler.cpp	\
	$(top_srcdir)/src/database/MediaDirectory.cpp		\
	$(top_srcdir)/src/database/Release.cpp		\
	$(top_srcdir)/src/database/SearchFilter.cpp	\
	$(top_srcdir)/src/database/SqlQuery.cpp		\
	$(top_srcdir)/src/database/User.cpp			\
	$(top_srcdir)/src/database/Video.cpp

database_integrity_CXXFLAGS=-std=c++11 -Wall -Wextra -DBOOST_LOG_DYN_LINK -I$(top_srcdir)/src

sql_query_SOURCES = \
	$(srcdir)/CheckSqlQuery.cpp         \
	$(top_srcdir)/src/database/SqlQuery.cpp

sql_query_CXXFLAGS=-std=c++11 -Wall -Wextra -I$(top_srcdir)/src

