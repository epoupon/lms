#include <boost/thread.hpp>

#include "DatabaseUpdateService.hpp"

namespace Service {

DatabaseUpdateService::DatabaseUpdateService(const Config& config)
: _metadataParser(),
 _databaseUpdater( config.dbPath, _metadataParser)
{
}

void
DatabaseUpdateService::start(void)
{
	_databaseUpdater.start();
}

void
DatabaseUpdateService::stop(void)
{
	std::cout << "DatabaseUpdateService::stop, processing..." << std::endl;
	_databaseUpdater.stop();
	std::cout << "DatabaseUpdateService::stop, process done" << std::endl;
}

void
DatabaseUpdateService::restart(void)
{
	std::cout << "DatabaseUpdateService::restart" << std::endl;
	stop();
	start();
}

} // namespace Service

