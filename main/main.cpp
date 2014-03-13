#include <boost/thread.hpp>

#include "LmsApplication.hpp"

#include "metadata/AvFormat.hpp"
#include "metadata/Extractor.hpp"
#include "database/Database.hpp"

#include "transcode/AvConvTranscoder.hpp"

#include "av/Common.hpp"

Wt::WApplication *createApplication(const Wt::WEnvironment& env)
{
	/*
	 * You could read information from the environment to decide whether
	 * the user has permission to start a new application
	 */
	  return new LmsApplication(env);
}

int main(int argc, char* argv[])
{

	Av::AvInit();
	Transcode::AvConvTranscoder::init();

//	std::locale::global(std::locale(""));
	MetaData::AvFormat metadataParser;
//	MetaData::Extractor metadataParser;

	// Set up the long living database session
	Database database("test.db", metadataParser);

//	database.watchDirectory( WatchedDirectory("/storage/common/Media/Son", WatchedDirectory::Audio ));
	database.watchDirectory(WatchedDirectory("/storage/common/Media/Son/Metal", WatchedDirectory::Audio));
//	database.watchDirectory("/storage/common/Media/Son/Metal/Iced Earth/2004 - The Glorious Burden");
//	database.watchDirectory("/storage/common/Media/Son/Metal/System Of a Down");
//	database.watchDirectory("/storage/common/Media/Son/Metal/Leprous");
//	database.watchDirectory("/storage/common/Media/Son/Electro");
//	database.watchDirectory( WatchedDirectory("/storage/common/Media/Son/Electro", WatchedDirectory::Audio) );
//	database.watchDirectory("/storage/common/Media/Son/Metal/Lacuna Coil");
	database.watchDirectory( WatchedDirectory("/storage/common/Media/Video", WatchedDirectory::Video) );
//	database.watchDirectory( WatchedDirectory("/storage/common/Media/Video/Films", WatchedDirectory::Video) );
//	database.watchDirectory( WatchedDirectory("/storage/common/Media/Video/Series", WatchedDirectory::Video) );

	std::cout << "Starting refresh..." << std::endl;
	boost::thread refreshThread(boost::bind(&Database::refresh, &database));


	return Wt::WRun(argc, argv, &createApplication);
}

