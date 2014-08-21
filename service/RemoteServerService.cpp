
#include "RemoteServerService.hpp"

namespace Service {

RemoteServerService::RemoteServerService(const Config& config)
: _server(boost::asio::ip::tcp::endpoint(config.address, config.port), config.dbPath)
{
}

void
RemoteServerService::start(void)
{
	std::cout << "RemoteServerService::start, starting..." << std::endl;
	_server.start();
	std::cout << "RemoteServerService::start, started!" << std::endl;
}


void
RemoteServerService::stop(void)
{
	std::cout << "RemoteServerService::stop, stopping..." << std::endl;
	_server.stop();
	std::cout << "RemoteServerService::stop, stopped!" << std::endl;
}

void
RemoteServerService::restart(void)
{
	std::cout << "RemoteServerService::restart, not implemented!" << std::endl;
}

} // namespace Service
