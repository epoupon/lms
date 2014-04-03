
#include "RemoteServerService.hpp"

RemoteServerService::RemoteServerService(const Remote::Server::Server::endpoint_type& endpoint)
: _server(endpoint)
{
}

void
RemoteServerService::start(void)
{
	std::cout << "émoteServerService::start, starting..." << std::endl;
	_server.run();
}


void
RemoteServerService::stop(void)
{
	std::cout << "émoteServerService::stop, stopping..." << std::endl;
	_server.stop();
}

void
RemoteServerService::restart(void)
{
	std::cout << "émoteServerService::restart, not implemented!" << std::endl;
}
