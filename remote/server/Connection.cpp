
#include <utility>
#include <vector>
#include <boost/bind.hpp>

#include "RequestHandler.hpp"

#include "ConnectionManager.hpp"
#include "Connection.hpp"

namespace Remote {
namespace Server {

Connection::Connection(boost::asio::ip::tcp::socket socket,
			ConnectionManager& manager,
			RequestHandler& handler)
: _socket(std::move(socket)),
_connectionManager(manager),
_requestHandler(handler)
{
}

boost::asio::ip::tcp::socket&
Connection::socket()
{
  return _socket;
}

void
Connection::start()
{
  _socket.async_read_some(boost::asio::buffer(_headerBuffer),
      boost::bind(&Connection::handleRead, shared_from_this(),
        boost::asio::placeholders::error,
        boost::asio::placeholders::bytes_transferred));
}

void
Connection::stop()
{
  _socket.close();
}

void
Connection::handleRead(const boost::system::error_code& error, std::size_t bytes_transferred)
{
	if (!error)
	{
/*			boost::asio::async_write(_socket, reply_.to_buffers(),
					boost::bind(&Connection::handleWrite, shared_from_this(),
						boost::asio::placeholders::error));*/


		start();
	}
	else if (error != boost::asio::error::operation_aborted)
	{
		std::cerr << "Connection::handleRead: " << error.message() << std::endl;
		_connectionManager.stop(shared_from_this());
	}
}

void Connection::handleWrite(const boost::system::error_code& error)
{
	if (!error)
	{
		// Initiate graceful Connection closure.
		boost::system::error_code ignored_ec;
		_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
	}

	if (error != boost::asio::error::operation_aborted)
	{
		std::cerr << "Connection::handleWrite: " << error.message() << std::endl;
		_connectionManager.stop(shared_from_this());
	}
}

} // namespace Server
} // namespace Remote
