#include <boost/thread.hpp>

#include "logger/Logger.hpp"

#include "DatabaseUpdateService.hpp"

namespace Service {

DatabaseUpdateService::DatabaseUpdateService(const Config& config)
: _metadataParser(),
 _databaseUpdater( config.dbPath, _metadataParser)
{
	_databaseUpdater.setAudioExtensions(config.audioExtensions);
	_databaseUpdater.setVideoExtensions(config.videoExtensions);
}

void
DatabaseUpdateService::start(void)
{
	LMS_LOG(MOD_SERVICE, SEV_DEBUG) << "DatabaseUpdateService, starting...";
	_databaseUpdater.start();
}

void
DatabaseUpdateService::stop(void)
{
	LMS_LOG(MOD_SERVICE, SEV_DEBUG) << "DatabaseUpdateService, stopping...";
	_databaseUpdater.stop();
	LMS_LOG(MOD_SERVICE, SEV_DEBUG) << "DatabaseUpdateService, stopped";
}

void
DatabaseUpdateService::restart(void)
{
	LMS_LOG(MOD_SERVICE, SEV_DEBUG) << "DatabaseUpdateService, restart";
	stop();
	start();
}

} // namespace Service

