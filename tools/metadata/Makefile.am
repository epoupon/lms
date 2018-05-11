bin_PROGRAMS = lms-metadata

lms_metadata_SOURCES = \
	$(srcdir)/LmsMetadata.cpp			\
	$(top_srcdir)/src/utils/Logger.cpp 		\
	$(top_srcdir)/src/utils/Utils.cpp 		\
	$(top_srcdir)/src/metadata/TagLibParser.cpp	\
	$(top_srcdir)/src/metadata/MetaData.cpp

lms_metadata_CXXFLAGS=-std=c++14 -Wall -I$(top_srcdir)/src -D_REENTRANT

