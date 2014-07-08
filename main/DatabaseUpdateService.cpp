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
	_thread = boost::thread(boost::bind(&DatabaseUpdater::Updater::process, &_databaseUpdater));
}

void
DatabaseUpdateService::stop(void)
{
	std::cout << "DatabaseUpdateService::stop, processing..." << std::endl;
	_thread.interrupt();
	_thread.join();
	std::cout << "DatabaseUpdateService::stop, process done" << std::endl;
}

void
DatabaseUpdateService::restart(void)
{
	std::cout << "DatabaseUpdateService::restart, not implemented" << std::endl;
}


