
#include "RemoteServerService.hpp"

RemoteServerService::RemoteServerService(boost::asio::io_service& ioService, const Remote::Server::Server::endpoint_type& endpoint, boost::filesystem::path dbPath)
: _server(ioService, endpoint, dbPath)
{
}

void
RemoteServerService::start(void)
{
	std::cout << "RemoteServerService::start, starting..." << std::endl;
	_server.run();
}


void
RemoteServerService::stop(void)
{
	std::cout << "RemoteServerService::stop, stopping..." << std::endl;
	_server.stop();
}

void
RemoteServerService::restart(void)
{
	std::cout << "RemoteServerService::restart, not implemented!" << std::endl;
}
