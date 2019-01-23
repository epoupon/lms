bin_PROGRAMS = lms-feature-extractor

lms_feature_extractor_SOURCES = \
	$(srcdir)/LmsFeatureExtractor.cpp		\
	$(top_srcdir)/src/database/Artist.cpp		\
	$(top_srcdir)/src/database/Cluster.cpp		\
	$(top_srcdir)/src/database/DatabaseHandler.cpp	\
	$(top_srcdir)/src/database/TrackFeature.cpp	\
	$(top_srcdir)/src/database/TrackList.cpp	\
	$(top_srcdir)/src/database/Release.cpp		\
	$(top_srcdir)/src/database/ScanSettings.cpp	\
	$(top_srcdir)/src/database/SqlQuery.cpp		\
	$(top_srcdir)/src/database/Track.cpp		\
	$(top_srcdir)/src/database/User.cpp		\
	$(top_srcdir)/src/utils/Config.cpp 		\
	$(top_srcdir)/src/utils/Logger.cpp 		\
	$(top_srcdir)/src/utils/Utils.cpp

lms_feature_extractor_CXXFLAGS=-std=c++14 -Wall -I$(top_srcdir)/src -D_REENTRANT

