
#include "DatabaseRefreshService.hpp"

DatabaseRefreshService::DatabaseRefreshService(boost::asio::io_service& ioService, const boost::filesystem::path& p)
: _metadataParser(),
 _database( p, _metadataParser)
{

	// TODO read from the database itself!
	// Move this code in the database class
	_database.watchDirectory( WatchedDirectory("/storage/common/Media/Son/Metal", WatchedDirectory::Audio) );
	_database.watchDirectory( WatchedDirectory("/storage/common/Media/Video", WatchedDirectory::Video) );

	// TODO launch thread
	//	boost::thread refreshThread(boost::bind(&Database::refresh, &database));
}

void
DatabaseRefreshService::start(void)
{
	std::cout << "DatabaseRefreshService::start, not implemented" << std::endl;

}

void
DatabaseRefreshService::stop(void)
{
	std::cout << "DatabaseRefreshService::stop, not implemented" << std::endl;
}

void
DatabaseRefreshService::restart(void)
{
	std::cout << "DatabaseRefreshService::restart, not implemented" << std::endl;
}


