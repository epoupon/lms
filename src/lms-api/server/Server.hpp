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

#ifndef REMOTE_SERVER_HPP
#define REMOTE_SERVER_HPP

#include <Wt/WIOService>
#include <Wt/Dbo/SqlConnectionPool>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <boost/filesystem.hpp>

#include "Connection.hpp"
#include "ConnectionManager.hpp"

#include "RequestHandler.hpp"

namespace LmsAPI {
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
				Wt::Dbo::SqlConnectionPool& connectionPool);

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
		Wt::Dbo::SqlConnectionPool& _connectionPool;
};

} // namespace Server
} // namespace LmsAPI

#endif
