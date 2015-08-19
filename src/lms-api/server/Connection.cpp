/*
 * Copyright (C) 2013 Emeric Poupon
 *
 * This file is part of LMS.
 *
 * LMS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LMS.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <utility>
#include <vector>

#include <boost/bind.hpp>
#include <boost/foreach.hpp>

#include "logger/Logger.hpp"

#include "messages.pb.h"

#include "RequestHandler.hpp"

#include "ConnectionManager.hpp"
#include "Connection.hpp"

namespace LmsAPI {
namespace Server {

Connection::Connection(boost::asio::io_service& ioService,
			boost::asio::ssl::context& context,
			ConnectionManager& manager,
			Wt::Dbo::SqlConnectionPool& connectionPool)
: _closing(false),
_socket(ioService, context),
_connectionManager(manager),
_requestHandler(connectionPool)
{
	LMS_LOG(MOD_REMOTE, SEV_DEBUG) << "Server::Connection::Connection, Creating connection";
}

void
Connection::start()
{
	LMS_LOG(MOD_REMOTE, SEV_DEBUG) << "Starting connection...";
	_socket.async_handshake(boost::asio::ssl::stream_base::server,
			boost::bind(&Connection::handleHandshake, this,
				boost::asio::placeholders::error));
}

void
Connection::handleHandshake(const boost::system::error_code& error)
{
	if (!error)
	{
		LMS_LOG(MOD_REMOTE, SEV_DEBUG) << "Handshake successfully performed... Now reading messages";
		readMsg();
	}
	else if (error != boost::asio::error::operation_aborted)
	{
		LMS_LOG(MOD_REMOTE, SEV_ERROR) << "Connection::handleHandshake: " << error.message();
		_connectionManager.stop(shared_from_this());
	}
	else
		LMS_LOG(MOD_REMOTE, SEV_ERROR) << "Handshake error: " << error.message();
}


void
Connection::readMsg()
{
	// Read a header first
	boost::asio::streambuf::mutable_buffers_type bufs = _inputStreamBuf.prepare(LmsAPI::Header::size);

	boost::asio::async_read(_socket,
			bufs,
			boost::asio::transfer_exactly(LmsAPI::Header::size),
			boost::bind(&Connection::handleReadHeader, shared_from_this(),
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred));
}

void
Connection::stop()
{
	if (!_closing)
	{
		boost::system::error_code ec;

		_closing = true;
		LMS_LOG(MOD_REMOTE, SEV_DEBUG) << "Server::Connection::stop, Stopping connection " << this;
		_socket.shutdown(ec);

		if (ec)
			LMS_LOG(MOD_REMOTE, SEV_ERROR) << "Error while shutting down connection " << this << ": " << ec.message();

		LMS_LOG(MOD_REMOTE, SEV_DEBUG) << "Server::Connection::stop, connection stopped " << this;
	}
	else
		LMS_LOG(MOD_REMOTE, SEV_DEBUG) << "Stop: close already in progress...";
}

void
Connection::handleReadHeader(const boost::system::error_code& error, std::size_t bytes_transferred)
{
	if (!error)
	{
		if (bytes_transferred != LmsAPI::Header::size)
		{
			LMS_LOG(MOD_REMOTE, SEV_ERROR) << "bytes_transferred (" << bytes_transferred << ") != LmsAPI::Header::size!";
			_connectionManager.stop(shared_from_this());
			return;
		}

		_inputStreamBuf.commit(bytes_transferred);

		std::istream is(&_inputStreamBuf);

		LmsAPI::Header header;
		if (!header.from_istream(is))
		{
			LMS_LOG(MOD_REMOTE, SEV_ERROR) << "Cannot read header from buffer!";
			_connectionManager.stop(shared_from_this());
			return;
		}

		// Now read the real message payload
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
		LMS_LOG(MOD_REMOTE, SEV_ERROR) << "Connection::handleReadHeader: " << error.message();
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

		LmsAPI::ServerMessage response;
		LmsAPI::ClientMessage request;

		if (!request.ParseFromIstream(&is))
		{
			LMS_LOG(MOD_REMOTE, SEV_ERROR) << "Parse request failed!";
			_connectionManager.stop(shared_from_this());
			return;
		}

		if (!_requestHandler.process(request, response))
		{
			LMS_LOG(MOD_REMOTE, SEV_ERROR) << "Process request failed!";
			_connectionManager.stop(shared_from_this());
			return;
		}

		{
			boost::system::error_code	ec;

			if (!response.SerializeToOstream(&os))
			{
				LMS_LOG(MOD_REMOTE, SEV_ERROR) << "Cannot serialize to ostream!";
				_connectionManager.stop(shared_from_this());
				return;
			}

			if (_outputStreamBuf.size() >= LmsAPI::Header::max_data_size)
			{
				LMS_LOG(MOD_REMOTE, SEV_ERROR) << "output message is too big! " << _outputStreamBuf.size() << " > " << LmsAPI::Header::max_data_size;
				_connectionManager.stop(shared_from_this());
				return;
			}

			std::array<unsigned char, LmsAPI::Header::size> headerBuffer;
			{
				LmsAPI::Header header;
				header.setDataSize(_outputStreamBuf.size());
				header.to_buffer(headerBuffer);
			}

			std::size_t n = boost::asio::write(_socket,
								boost::asio::buffer(headerBuffer),
								boost::asio::transfer_exactly(LmsAPI::Header::size),
								ec);
			if (ec)
			{
				LMS_LOG(MOD_REMOTE, SEV_ERROR) << "cannot write header: " << error.message();
				_connectionManager.stop(shared_from_this());
			}
			else
			{
				assert(n == LmsAPI::Header::size);
			}

			// Now send serialized payload
			n = boost::asio::write(_socket,
					_outputStreamBuf.data(),
					boost::asio::transfer_exactly(_outputStreamBuf.size()),
					ec);

			if (ec)
			{
				LMS_LOG(MOD_REMOTE, SEV_ERROR) << "cannot write msg: " << error.message();
				_connectionManager.stop(shared_from_this());
			}
			else
			{
				assert(n == _outputStreamBuf.size());
				_outputStreamBuf.consume(n);
			}

		}

		// All good here, read another message
		readMsg();
	}
	else if (error != boost::asio::error::operation_aborted)
	{
		LMS_LOG(MOD_REMOTE, SEV_ERROR) << "Connection::handleRead: " << error.message();
		_connectionManager.stop(shared_from_this());
	}
}


} // namespace Server
} // namespace LmsAPI
