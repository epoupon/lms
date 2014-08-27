#include "logger/Logger.hpp"

#include "RemoteServerService.hpp"

namespace Service {

RemoteServerService::RemoteServerService(const Config& config)
: _server(boost::asio::ip::tcp::endpoint(config.address, config.port),
	config.sslCertificatePath,
	config.sslPrivateKeyPath,
	config.sslTempDhPath,
	config.dbPath)
{
}

void
RemoteServerService::start(void)
{
	LMS_LOG(MOD_SERVICE, SEV_DEBUG) << "RemoteServerService::start, starting...";
	_server.start();
	LMS_LOG(MOD_SERVICE, SEV_DEBUG) << "RemoteServerService::start, started!";
}


void
RemoteServerService::stop(void)
{
	LMS_LOG(MOD_SERVICE, SEV_DEBUG) << "RemoteServerService::stop, stopping...";
	_server.stop();
	LMS_LOG(MOD_SERVICE, SEV_DEBUG) << "RemoteServerService::stop, stopped!";
}

void
RemoteServerService::restart(void)
{
	LMS_LOG(MOD_SERVICE, SEV_DEBUG) << "RemoteServerService::restart, not implemented!";
}

} // namespace Service
