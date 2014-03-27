#ifndef REMOTE_CONNECTION_HPP
#define REMOTE_CONNECTION_HPP

#include <memory>
#include <array.hpp>

#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>

#include <boost/enable_shared_from_this.hpp>

#include "reply.hpp"
#include "request.hpp"
#include "RequestHandler.hpp"
#include "request_parser.hpp"

namespace http {
namespace server {

class ConnectionManager;

/// Represents a single connection from a client.
class Connection : public boost::enable_shared_from_this<Connection>, boost::noncopyable
{
	public:

		typedef std::shared_ptr<connection> pointer;

		/// Construct a connection with the given io_service.
		Connection(boost::asio::io_service& io_service,
				ConnectionManager& manager, RequestHandler& handler);

		boost::asio::ip::tcp::socket& socket();

		/// Start the first asynchronous operation for the connection.
		void start();

		/// Stop all asynchronous operations associated with the connection.
		void stop();

	private:
		/// Handle completion of a read operation.
		void handleRead(const boost::system::error_code& e,
				std::size_t bytes_transferred);

		/// Handle completion of a write operation.
		void handleWrite(const boost::system::error_code& e);

		/// Socket for the connection.
		boost::asio::ip::tcp::socket _socket;

		/// The manager for this connection.
		ConnectionManager& _ConnectionManager;

		/// The handler used to process the incoming requests.
		RequestHandler& _RequestHandler;

		// TODO use streambuffers

		/// The incoming request.
//		request request_;

		/// The parser for the incoming request.
//		request_parser request_parser_;

		/// The reply to be sent back to the client.
//		reply reply_;

		std::array<unsigned char, Header::size>	_headerBuffer;
};


} // namespace server
} // namespace http

#endif // HTTP_CONNECTION_HPP


