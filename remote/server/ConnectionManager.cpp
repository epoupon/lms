#include <algorithm>

#include <boost/foreach.hpp>

#include "ConnectionManager.hpp"

namespace Remote {
namespace Server {

ConnectionManager::ConnectionManager()
{
}


void
ConnectionManager::start(Connection::pointer c)
{
	_connections.insert(c);
	c->start();
}

void
ConnectionManager::stop(Connection::pointer c)
{
	_connections.erase(c);
	c->stop();
}

void
ConnectionManager::stopAll()
{
	BOOST_FOREACH(Connection::pointer c, _connections)
	{
		c->stop();
	}
	_connections.clear();
}


} // namespace Server
} // namespace Remote


