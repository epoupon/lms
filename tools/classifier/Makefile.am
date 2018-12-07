bin_PROGRAMS = lms-classifier

lms_classifier_SOURCES = \
	$(srcdir)/LmsClassifier.cpp			\
	$(top_srcdir)/src/classifier/DataNormalizer.cpp	\
	$(top_srcdir)/src/classifier/SOM.cpp		\
	$(top_srcdir)/src/utils/Logger.cpp 		\
	$(top_srcdir)/src/utils/Utils.cpp

lms_classifier_CXXFLAGS=-std=c++14 -Wall -I$(top_srcdir)/src -D_REENTRANT

