#ifndef REMOTE_CONNECTION_MANAGER_HPP
#define REMOTE_CONNECTION_MANAGER_HPP

#include <set>

#include "Connection.hpp"

namespace Remote {
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
} // namespace Remote

#endif
