
#include <utility>

#include <boost/bind.hpp>

#include "Server.hpp"

namespace Remote {
namespace Server {

Server::Server(boost::asio::io_service& ioService, const endpoint_type& bindEndpoint, boost::filesystem::path dbPath)
:
_ioService(ioService),
_acceptor(_ioService, bindEndpoint, true /*SO_REUSEADDR*/),
_connectionManager(),
_socket(_ioService),
_requestHandler(dbPath)
{
}

void
Server::run()
{
	// While the server is running, there is always at least one
	// asynchronous operation outstanding: the asynchronous accept call waiting
	// for new incoming connections.
	asyncAccept();
}

void
Server::asyncAccept()
{
	_acceptor.async_accept(_socket, boost::bind(&Server::handleAccept, this, boost::asio::placeholders::error));
}

void
Server::handleAccept(boost::system::error_code ec)
{
	// Check whether the server was stopped before this
	// completion handler had a chance to run.
	if (!_acceptor.is_open())
	{
		return;
	}

	if (!ec)
	{
		_connectionManager.start(std::make_shared<Connection>(std::move(_socket), _connectionManager, _requestHandler));

		asyncAccept();
	}
	else
		std::cerr << "handleAccept: " << ec.message() << std::endl;
}


void
Server::stop()
{
	// The server is stopped by cancelling all outstanding asynchronous
	// operations.
	_acceptor.close();
	_connectionManager.stopAll();
}

} // namespace Server
} // namespace Remote
