
#include <utility>

#include <boost/bind.hpp>

#include "Server.hpp"

namespace Remote {

namespace Server {

server::server(boost::asio::io_service& ioService, const endpoint_type& endpoint)
: _ioService(ioService),
_acceptor(_ioService),
_connectionManager(),
_socket(io_service_),
_requestHandler(doc_root)
{
	// Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
	boost::asio::ip::tcp::resolver resolver(_ioService);
	boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(endpoint);

	_acceptor.open(endpoint.protocol());
	_acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
	_acceptor.bind(endpoint);
	_acceptor.listen();

}

void
Server::run()
{
	// While the server is running, there is always at least one
	// asynchronous operation outstanding: the asynchronous accept call waiting
	// for new incoming connections.
	async_accept();
}

void
Server::async_accept()
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
		_connectionManager.start(std::make_shared<connection>(std::move(_socket), _connectionManager, _requestHandler));

		async_accept();
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
	_connectionManager.stop_all();
}

} // namespace Server

} // namespace Remote
