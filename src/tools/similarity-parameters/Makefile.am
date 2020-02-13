noinst_PROGRAMS = lms-similarity-parameters

lms_similarity_parameters_SOURCES = \
	$(srcdir)/LmsSimilarityParameters.cpp			\
	$(top_srcdir)/src/database/Artist.cpp		\
	$(top_srcdir)/src/database/Cluster.cpp		\
	$(top_srcdir)/src/database/Db.cpp		\
	$(top_srcdir)/src/database/TrackFeatures.cpp	\
	$(top_srcdir)/src/database/TrackList.cpp	\
	$(top_srcdir)/src/database/Release.cpp		\
	$(top_srcdir)/src/database/ScanSettings.cpp	\
	$(top_srcdir)/src/database/Session.cpp		\
	$(top_srcdir)/src/database/SessionPool.cpp	\
	$(top_srcdir)/src/database/SqlQuery.cpp		\
	$(top_srcdir)/src/database/Track.cpp		\
	$(top_srcdir)/src/database/User.cpp		\
	$(top_srcdir)/src/similarity/features/som/DataNormalizer.cpp		\
	$(top_srcdir)/src/similarity/features/som/Network.cpp			\
	$(top_srcdir)/src/similarity/features/SimilarityFeaturesCache.cpp	\
	$(top_srcdir)/src/similarity/features/SimilarityFeaturesSearcher.cpp	\
	$(top_srcdir)/src/similarity/features/SimilarityFeaturesDefs.cpp	\
	$(top_srcdir)/src/utils/Config.cpp 		\
	$(top_srcdir)/src/utils/Logger.cpp 		\
	$(top_srcdir)/src/utils/Random.cpp 		\
	$(top_srcdir)/src/utils/StreamLogger.cpp 	\
	$(top_srcdir)/src/utils/String.cpp

lms_similarity_parameters_CXXFLAGS=-std=c++17 -I$(top_srcdir)/src -D_REENTRANT

