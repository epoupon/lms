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

#include <algorithm>

#include <boost/foreach.hpp>

#include "ConnectionManager.hpp"

namespace LmsAPI {
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
} // namespace LmsAPI


