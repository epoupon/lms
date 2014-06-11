
#include <utility>
#include <vector>

#include <boost/bind.hpp>
#include <boost/foreach.hpp>

#include "messages/messages.pb.h"

#include "RequestHandler.hpp"

#include "ConnectionManager.hpp"
#include "Connection.hpp"

namespace Remote {
namespace Server {

Connection::Connection(boost::asio::ip::tcp::socket socket,
			ConnectionManager& manager,
			RequestHandler& handler)
: _closing(false),
_socket(std::move(socket)),
_connectionManager(manager),
_requestHandler(handler)
{
	std::cout << "Server::Connection::Connection, Creating connection" << std::endl;
}

boost::asio::ip::tcp::socket&
Connection::socket()
{
  return _socket;
}

void
Connection::start()
{
	boost::asio::streambuf::mutable_buffers_type bufs = _inputStreamBuf.prepare(Remote::Header::size);

	boost::asio::async_read(_socket,
			bufs,
			boost::asio::transfer_exactly(Remote::Header::size),
			boost::bind(&Connection::handleReadHeader, shared_from_this(),
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred));
}

void
Connection::stop()
{
	if (!_closing)
	{
		_closing = true;
		std::cout << "Server::Connection::stop, Stopping connection " << this << std::endl;
		_socket.close();
		std::cout << "Server::Connection::stop, connection stopped " << this << std::endl;
	}
	else
		std::cout << "Close in progress..." << std::endl;
}

void
Connection::handleReadHeader(const boost::system::error_code& error, std::size_t bytes_transferred)
{
	if (!error)
	{
		if (bytes_transferred != Remote::Header::size)
		{
			std::cerr << "bytes_transferred (" << bytes_transferred << ") != Remote::Header::size!" << std::endl;
			_connectionManager.stop(shared_from_this());
			return;
		}

		_inputStreamBuf.commit(bytes_transferred);

		std::istream is(&_inputStreamBuf);

		Remote::Header header;
		if (!header.from_istream(is))
		{
			std::cerr << "Cannot read header from buffer!" << std::endl;
			_connectionManager.stop(shared_from_this());
			return;
		}

		// Now read the real message
		boost::asio::streambuf::mutable_buffers_type bufs = _inputStreamBuf.prepare(header.getDataSize());

		boost::asio::async_read(_socket,
				bufs,
				boost::asio::transfer_exactly(header.getDataSize()),
				boost::bind(&Connection::handleReadMsg, shared_from_this(),
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred));

	}
	else if (error != boost::asio::error::operation_aborted)
	{
		std::cerr << "Connection::handleReadHeader: " << error.message() << std::endl;
		_connectionManager.stop(shared_from_this());
	}
}

void
Connection::handleReadMsg(const boost::system::error_code& error, std::size_t bytes_transferred)
{
	if (!error)
	{
		_inputStreamBuf.commit(bytes_transferred);

		std::istream is(&_inputStreamBuf);
		std::ostream os(&_outputStreamBuf);

		Remote::ServerMessage response;
		Remote::ClientMessage request;

		if (!request.ParseFromIstream(&is))
		{
			std::cerr << "Cannot parse request!" << std::endl;
			_connectionManager.stop(shared_from_this());
			return;
		}

		if (!_requestHandler.process(request, response))
		{
			std::cerr << "Cannot process request!" << std::endl;
			_connectionManager.stop(shared_from_this());
			return;
		}

		{
			boost::system::error_code	ec;

			if (!response.SerializeToOstream(&os))
			{
				std::cerr << "Cannot serialize to ostream!" << std::endl;
				_connectionManager.stop(shared_from_this());
				return;
			}

			if (_outputStreamBuf.size() >= Remote::Header::max_data_size)
			{
				std::cerr << "output message is too big! " << _outputStreamBuf.size() << " > " << Remote::Header::max_data_size << std::endl;
				_connectionManager.stop(shared_from_this());
				return;
			}

			std::array<unsigned char, Remote::Header::size> headerBuffer;
			{
				Remote::Header header;
				header.setDataSize(_outputStreamBuf.size());
				header.to_buffer(headerBuffer);
			}

			std::size_t n = boost::asio::write(_socket,
								boost::asio::buffer(headerBuffer),
								boost::asio::transfer_exactly(Remote::Header::size),
								ec);
			if (ec)
			{
				std::cerr << "cannot write header: " << error.message() << std::endl;
				_connectionManager.stop(shared_from_this());
			}

			// Now send serialized payload
			n = boost::asio::write(_socket,
					_outputStreamBuf.data(),
					boost::asio::transfer_exactly(_outputStreamBuf.size()),
					ec);

			assert(n == _outputStreamBuf.size());

			_outputStreamBuf.consume(n);

			if (ec)
			{
				std::cerr << "cannot write msg: " << error.message() << std::endl;
				_connectionManager.stop(shared_from_this());
			}
		}
		start();

		// Initiate graceful Connection closure.
		//		boost::system::error_code ignored_ec;
		//		_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);

	}
	else if (error != boost::asio::error::operation_aborted)
	{
		std::cerr << "Connection::handleRead: " << error.message() << std::endl;
		_connectionManager.stop(shared_from_this());
	}
}


} // namespace Server
} // namespace Remote
