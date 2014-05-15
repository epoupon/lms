#ifndef REMOTE_CONNECTION_HPP
#define REMOTE_CONNECTION_HPP

#include <array>
#include <memory>

#include <boost/asio.hpp>

#include "RequestHandler.hpp"

#include "messages/Header.hpp"

namespace Remote {
namespace Server {

class ConnectionManager;

/// Represents a single connection from a client.
class Connection : public std::enable_shared_from_this<Connection>
{
	public:

		Connection(const Connection&) = delete;
		Connection& operator=(const Connection&) = delete;

		typedef std::shared_ptr<Connection> pointer;

		/// Construct a connection with the given io_service.
		explicit Connection(boost::asio::ip::tcp::socket socket,
				ConnectionManager& manager, RequestHandler& handler);

		boost::asio::ip::tcp::socket& socket();

		/// Start the first asynchronous operation for the connection.
		void start();

		/// Stop all asynchronous operations associated with the connection.
		void stop();

	private:
		/// Handle completion of a read operation.
		void handleReadHeader(const boost::system::error_code& e,
					std::size_t bytes_transferred);

		void handleReadMsg(const boost::system::error_code& e,
					std::size_t bytes_transferred);

		/// Socket for the connection.
		boost::asio::ip::tcp::socket _socket;

		/// The manager for this connection.
		ConnectionManager& _connectionManager;

		/// The handler used to process the incoming requests.
		RequestHandler& _requestHandler;

		boost::asio::streambuf _inputStreamBuf;
		boost::asio::streambuf _outputStreamBuf;
};


} // namespace Server
} // namespace Remote

#endif


