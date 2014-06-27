#include <boost/thread.hpp>

#include "DatabaseRefreshService.hpp"

DatabaseRefreshService::DatabaseRefreshService(boost::asio::io_service& ioService, const boost::filesystem::path& p)
: _metadataParser(),
 _database( p, _metadataParser)
{
	// TODO read from the database itself!
	// Move this code in the database class
	_database.watchDirectory( Database::WatchedDirectory("/storage/common/Media/Son/Metal", Database::WatchedDirectory::Audio) );
	_database.watchDirectory( Database::WatchedDirectory("/storage/common/Media/Video", Database::WatchedDirectory::Video) );

}

void
DatabaseRefreshService::start(void)
{
//	_thread = boost::thread(boost::bind(&Database::refresh, &_database));
}

void
DatabaseRefreshService::stop(void)
{
	std::cout << "DatabaseRefreshService::stop, processing..." << std::endl;
	_thread.interrupt();
	_thread.join();
	std::cout << "DatabaseRefreshService::stop, process done" << std::endl;
}

void
DatabaseRefreshService::restart(void)
{
	std::cout << "DatabaseRefreshService::restart, not implemented" << std::endl;
}


