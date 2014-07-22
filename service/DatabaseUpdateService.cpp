#include <boost/thread.hpp>

#include "DatabaseUpdateService.hpp"

DatabaseUpdateService::DatabaseUpdateService(boost::asio::io_service& ioService, const boost::filesystem::path& p)
: _metadataParser(),
 _databaseUpdater( p, _metadataParser)
{
}

void
DatabaseUpdateService::start(void)
{
	// TODO
	// Read database parameters and program a timer for the next scan
//	_thread = boost::thread(boost::bind(&DatabaseUpdater::Updater::process, &_databaseUpdater));
}

void
DatabaseUpdateService::stop(void)
{
	std::cout << "DatabaseUpdateService::stop, processing..." << std::endl;
	// no effect if thread does not exist
	_thread.interrupt();
	_thread.join();
	std::cout << "DatabaseUpdateService::stop, process done" << std::endl;
}

void
DatabaseUpdateService::restart(void)
{
	std::cout << "DatabaseUpdateService::restart" << std::endl;
	stop();
	start();
}

bool
DatabaseUpdateService::isScanning(void) const
{
	// scanning is active only if a thread is running the updater
	return _thread.get_id() != boost::thread::id();
}


