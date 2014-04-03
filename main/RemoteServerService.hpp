#ifndef REMOTE_SERVER_SERVICE_HPP
#define REMOTE_SERVER_SERVICE_HPP


#include "Service.hpp"

#include "remote/server/Server.hpp"

class RemoteServerService : public Service
{
	public:

		RemoteServerService(const Remote::Server::Server::endpoint_type& endpoint);

		void start(void);
		void stop(void);
		void restart(void);

	private:

		Remote::Server::Server	_server;
};

#endif
