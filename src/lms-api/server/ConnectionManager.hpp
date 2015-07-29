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

#ifndef REMOTE_CONNECTION_MANAGER_HPP
#define REMOTE_CONNECTION_MANAGER_HPP

#include <set>

#include "Connection.hpp"

namespace LmsAPI {
namespace Server {

/// Manages open connections so that they may be cleanly stopped when the server
/// needs to shut down.

class ConnectionManager
{
	public:
		ConnectionManager(const ConnectionManager&) = delete;
		ConnectionManager& operator=(const ConnectionManager&) = delete;

		ConnectionManager();

		/// Add the specified connection to the manager and start it.
		void start(Connection::pointer c);

		/// Stop the specified connection.
		void stop(Connection::pointer c);

		/// Stop all connections.
		void stopAll();

	private:
		/// The managed connections.
		std::set<Connection::pointer> _connections;
};

} // namespace Server
} // namespace LmsAPI

#endif
