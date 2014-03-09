
TESTS = database-integrity sql-query database-basics

check_PROGRAMS = database-integrity sql-query database-basics

database_integrity_SOURCES = \
	$(srcdir)/DatabaseIntegrity.cpp		\
	$(top_srcdir)/database/Artist.cpp	\
	$(top_srcdir)/database/Genre.cpp	\
	$(top_srcdir)/database/Release.cpp	\
	$(top_srcdir)/database/Track.cpp	\
	$(top_srcdir)/database/DatabaseHandler.cpp	\
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
	$(top_srcdir)/database/Path.cpp			\
	$(top_srcdir)/database/Video.cpp			\
	$(top_srcdir)/database/Checksum.cpp

database_basics_CXXFLAGS=-std=c++11 -Wall -Wextra -I$(top_srcdir)


sql_query_SOURCES = \
	$(srcdir)/CheckSqlQuery.cpp         \
	$(top_srcdir)/database/SqlQuery.cpp

sql_query_CXXFLAGS=-std=c++11 -Wall -Wextra -I$(top_srcdir)

