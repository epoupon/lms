#ifndef REMOTE_SERVER_SERVICE_HPP
#define REMOTE_SERVER_SERVICE_HPP

#include <boost/filesystem.hpp>

#include "Service.hpp"

#include "remote/server/Server.hpp"

namespace Service {

class RemoteServerService : public Service
{
	public:

		RemoteServerService(const Remote::Server::Server::endpoint_type& endpoint, boost::filesystem::path dbPath);

		void start(void);
		void stop(void);
		void restart(void);

	private:

		Remote::Server::Server	_server;
};

} // namespace Service

#endif
