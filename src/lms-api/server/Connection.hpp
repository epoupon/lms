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

#ifndef LMSAPI_CONNECTION_HPP
#define LMSAPI_CONNECTION_HPP

#include <array>
#include <memory>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include "RequestHandler.hpp"

#include "messages/Header.hpp"

namespace LmsAPI {
namespace Server {

class ConnectionManager;

/// Represents a single connection from a client.
class Connection : public std::enable_shared_from_this<Connection>
{
	public:

		typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> 	ssl_socket;

		Connection(const Connection&) = delete;
		Connection& operator=(const Connection&) = delete;

		typedef std::shared_ptr<Connection> pointer;

		/// Construct a connection with the given io_service.
		explicit Connection(boost::asio::io_service& ioService, boost::asio::ssl::context& context,
				ConnectionManager& manager,
				const boost::filesystem::path& dbPath);

		ssl_socket::lowest_layer_type& getSocket()	{return _socket.lowest_layer();}

		/// Start the first asynchronous operation for the connection.
		void start();

		/// Stop all asynchronous operations associated with the connection.
		void stop();

	private:
		bool _closing;

		/// Read a new message on the the connection
		void readMsg();

		/// Handle completion of ssl handshake
		void handleHandshake(const boost::system::error_code& error);

		/// Handle completion of a read operation.
		void handleReadHeader(const boost::system::error_code& e,
					std::size_t bytes_transferred);

		void handleReadMsg(const boost::system::error_code& e,
					std::size_t bytes_transferred);

		/// Socket for the connection.
		ssl_socket _socket;

		/// The manager for this connection.
		ConnectionManager& _connectionManager;

		/// The handler used to process the incoming requests.
		RequestHandler	_requestHandler;

		boost::asio::streambuf _inputStreamBuf;
		boost::asio::streambuf _outputStreamBuf;
};


} // namespace Server
} // namespace LmsAPI

#endif


