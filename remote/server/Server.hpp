#ifndef REMOTE_SERVER_HPP
#define REMOTE_SERVER_HPP

#include <Wt/WIOService>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <boost/filesystem.hpp>

#include "Connection.hpp"
#include "ConnectionManager.hpp"

#include "RequestHandler.hpp"

namespace Remote {
namespace Server {

class Server
{
	public:

		Server(const Server&) = delete;
		Server& operator=(const Server&) = delete;

		typedef boost::asio::ip::tcp::endpoint 	endpoint_type;

		// Serve up data from the given database
		Server(const endpoint_type& bindEndpoint,
				boost::filesystem::path certPath,
				boost::filesystem::path privKeyPath,
				boost::filesystem::path dhPath,
				boost::filesystem::path dbPath);

		// Run the server's io_service loop.
		void start();

		void stop();

	private:
		/// Perform an asynchronous accept operation.
		void asyncAccept();
		void handleAccept(std::shared_ptr<Connection> newConnection, boost::system::error_code ec);

		Wt::WIOService		_ioService;

		/// Acceptor used to listen for incoming connections.
		boost::asio::ip::tcp::acceptor _acceptor;

		/// The connection manager which owns all live connections.
		ConnectionManager _connectionManager;

		boost::asio::ssl::context _context;

		/// The database to be used for requests
		boost::filesystem::path _dbPath;
};

} // namespace Server
} // namespace Remote

#endif
