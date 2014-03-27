#ifndef REMOTE_CONNECTION_MANAGER_HPP
#define REMOTE_CONNECTION_MANAGER_HPP

#include <set>

#include <boost/noncopyable.hpp>

#include "connection.hpp"

namespace Remote {

namespace Server {

/// Manages open connections so that they may be cleanly stopped when the server
/// needs to shut down.

class ConnectionManager : boost::noncopyable
{
	public:
		/// Add the specified connection to the manager and start it.
		void start(connection::pointer c);

		/// Stop the specified connection.
		void stop(connection::pointer c);

		/// Stop all connections.
		void stop_all();

	private:
		/// The managed connections.
		std::set<connection::pointer> _connections;
};

} // namespace Server

} // namespace Remote

#endif
