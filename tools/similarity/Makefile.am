noinst_PROGRAMS = lms-similarity

lms_similarity_SOURCES = \
	$(srcdir)/LmsSimilarity.cpp			\
	$(top_srcdir)/src/database/Artist.cpp		\
	$(top_srcdir)/src/database/Cluster.cpp		\
	$(top_srcdir)/src/database/Database.cpp		\
	$(top_srcdir)/src/database/TrackFeatures.cpp	\
	$(top_srcdir)/src/database/TrackList.cpp	\
	$(top_srcdir)/src/database/Release.cpp		\
	$(top_srcdir)/src/database/ScanSettings.cpp	\
	$(top_srcdir)/src/database/Session.cpp		\
	$(top_srcdir)/src/database/SimilaritySettings.cpp	\
	$(top_srcdir)/src/database/SqlQuery.cpp		\
	$(top_srcdir)/src/database/Track.cpp		\
	$(top_srcdir)/src/database/User.cpp		\
	$(top_srcdir)/src/similarity/features/som/DataNormalizer.cpp		\
	$(top_srcdir)/src/similarity/features/som/Network.cpp			\
	$(top_srcdir)/src/utils/Config.cpp 		\
	$(top_srcdir)/src/utils/Logger.cpp 		\
	$(top_srcdir)/src/utils/Utils.cpp

lms_similarity_CXXFLAGS=-std=c++14 -Wall -I$(top_srcdir)/src -D_REENTRANT

