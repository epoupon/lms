
#include <algorithm>
#include <boost/bind.hpp>

#include "ConnectionManager.hpp"

namespace Remote {

namespace Server {

void
ConnectionManager::start(connection::pointer c)
{
	_connections.insert(c);
	c->start();
}

void
ConnectionManager::stop(connection::pointer c)
{
	_connections.erase(c);
	c->stop();
}

void
ConnectionManager::stop_all()
{
	BOOST_FOREACH(connection::pointer c, _connections)
	{
		c->stop();
	}
	_connections.clear();
}


} // namespace Server

} // namespace Remote


