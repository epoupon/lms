#ifndef REMOTE_SERVER_HPP
#define REMOTE_SERVER_HPP

#include <boost/asio.hpp>

#include <string>

#include "Connection.hpp"
#include "ConnectionManager.hpp"

#include "RequestHandler.hpp"

namespace Remote {

namespace Server {

class Server
{
	public:

		typedef std::string	endpoint_type;

		// Serve up data from the given database
		Server(boost::asio::io_service& ioService,
				const endpoint_type& endpoint);

		// Run the server's io_service loop.
		void run();

		void stop();

	private:
		/// Perform an asynchronous accept operation.
		void asyncAccept();
		void handleAccept(boost::system::error_code ec);

		boost::asio::io_service& _ioService;

		/// Acceptor used to listen for incoming connections.
		boost::asio::ip::tcp::acceptor _acceptor;

		/// The connection manager which owns all live connections.
		connection_manager _connectionManager;

		/// The next socket to be accepted.
		boost::asio::ip::tcp::socket _socket;

		/// The handler for all incoming requests.
		RequestHandler _requestHandler;
};

} // namespace Server

} // namespace Remote

#endif
