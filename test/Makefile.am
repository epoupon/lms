
TESTS = sql-query database-user

check_PROGRAMS = sql-query database-user test-wt test-avmetadata

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

database_basics_CXXFLAGS=-std=c++11 -Wall -Wextra -I$(top_srcdir)/src

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

database_user_CXXFLAGS=-std=c++11 -Wall -Wextra -I$(top_srcdir)/src


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

database_integrity_CXXFLAGS=-std=c++11 -Wall -Wextra -I$(top_srcdir)/src

sql_query_SOURCES = \
	$(srcdir)/CheckSqlQuery.cpp         \
	$(top_srcdir)/src/database/SqlQuery.cpp

sql_query_CXXFLAGS=-std=c++11 -Wall -Wextra -I$(top_srcdir)/src


test_wt_SOURCES = TestWt.cpp
test_wt_CXXFLAGS=-std=c++11 -Wall -Wextra

test_wt_audio_SOURCES = TestWtAudio.cpp\
	$(top_srcdir)/src/logger/Logger.cpp 		\
	$(top_srcdir)/src/utils/Utils.cpp 		\
	$(top_srcdir)/src/metadata/AvFormat.cpp		\
	$(top_srcdir)/src/av/AvInfo.cpp			\
	$(top_srcdir)/src/av/AvTranscoder.cpp		\
	$(top_srcdir)/src/cover/CoverArtGrabber.cpp	\
	$(top_srcdir)/src/database/Artist.cpp		\
	$(top_srcdir)/src/database/Playlist.cpp		\
	$(top_srcdir)/src/database/Track.cpp		\
	$(top_srcdir)/src/database/DatabaseHandler.cpp	\
	$(top_srcdir)/src/database/MediaDirectory.cpp	\
	$(top_srcdir)/src/database/Release.cpp		\
	$(top_srcdir)/src/database/SearchFilter.cpp	\
	$(top_srcdir)/src/database/SqlQuery.cpp		\
	$(top_srcdir)/src/database/User.cpp		\
	$(top_srcdir)/src/database/Video.cpp		\
	$(top_srcdir)/src/image/Image.cpp		\
	$(top_srcdir)/src/ui/resource/CoverResource.cpp	\
	$(top_srcdir)/src/ui/resource/TranscodeResource.cpp

test_wt_audio_CXXFLAGS=-std=c++11 -Wall -Wextra -I$(top_srcdir)/src -I$(top_srcdir)/src/ui $(MAGICKXX_CFLAGS) 
test_wt_audio_LDADD=$(MAGICKXX_LIBS)


test_avmetadata_SOURCES = TestAvMetadata.cpp 		\
	$(top_srcdir)/src/logger/Logger.cpp 		\
	$(top_srcdir)/src/utils/Utils.cpp 		\
	$(top_srcdir)/src/metadata/AvFormat.cpp		\
	$(top_srcdir)/src/metadata/TagLibParser.cpp	\
	$(top_srcdir)/src/av/AvInfo.cpp

test_avmetadata_CXXFLAGS=-std=c++11 -Wall -Wextra  -I$(top_srcdir)/src


test_avtranscoder_SOURCES = TestAvTranscoder.cpp 		\
	$(top_srcdir)/src/logger/Logger.cpp 		\
	$(top_srcdir)/src/utils/Utils.cpp 		\
	$(top_srcdir)/src/av/AvInfo.cpp			\
	$(top_srcdir)/src/av/AvTranscoder.cpp

test_avtranscoder_CXXFLAGS=-std=c++11 -Wall -Wextra  -I$(top_srcdir)/src


