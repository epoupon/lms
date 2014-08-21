#ifndef REMOTE_SERVER_SERVICE_HPP
#define REMOTE_SERVER_SERVICE_HPP

#include <boost/filesystem.hpp>
#include <boost/asio/ip/address.hpp>

#include "Service.hpp"

#include "remote/server/Server.hpp"

namespace Service {

class RemoteServerService : public Service
{
	public:

		struct Config {
			bool				enable;
			boost::asio::ip::address	address;
			unsigned short			port;
			boost::filesystem::path		dbPath;
		};

		RemoteServerService(const Config& config);

		void start(void);
		void stop(void);
		void restart(void);

	private:

		Remote::Server::Server	_server;
};

} // namespace Service

#endif
