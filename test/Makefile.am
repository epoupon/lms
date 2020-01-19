
TESTS = test-som test-database

check_PROGRAMS = test-som test-database

test_som_SOURCES = \
	$(srcdir)/som/SomTest.cpp					\
	$(top_srcdir)/src/similarity/features/som/DataNormalizer.cpp	\
	$(top_srcdir)/src/similarity/features/som/Network.cpp

test_som_CXXFLAGS=-std=c++17 -I${top_srcdir}/src/ -I${top_srcdir}/src/similarity/features/som/


test_database_SOURCES = \
	$(srcdir)/database/DatabaseTest.cpp			\
	$(top_srcdir)/src/database/Artist.cpp			\
	$(top_srcdir)/src/database/Cluster.cpp			\
	$(top_srcdir)/src/database/Db.cpp			\
	$(top_srcdir)/src/database/TrackArtistLink.cpp		\
	$(top_srcdir)/src/database/TrackFeatures.cpp		\
	$(top_srcdir)/src/database/TrackList.cpp		\
	$(top_srcdir)/src/database/Release.cpp			\
	$(top_srcdir)/src/database/ScanSettings.cpp		\
	$(top_srcdir)/src/database/Session.cpp			\
	$(top_srcdir)/src/database/SqlQuery.cpp			\
	$(top_srcdir)/src/database/Track.cpp			\
	$(top_srcdir)/src/database/User.cpp			\
	$(top_srcdir)/src/utils/Logger.cpp			\
	$(top_srcdir)/src/utils/StreamLogger.cpp		\
	$(top_srcdir)/src/utils/Utils.cpp

test_database_CXXFLAGS=-std=c++17 -I${top_srcdir}/src/

