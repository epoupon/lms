/*
 * Copyright (C) 2019 Emeric Poupon
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

#include "SessionPool.hpp"

#include "utils/Exception.hpp"
#include "utils/Logger.hpp"

#include "Session.hpp"

namespace Database {

SessionPool::SessionPool(Db& database, std::size_t maxSessionCount)
: _db {database},
_maxSessionCount {maxSessionCount}
{
}

Session&
SessionPool::acquireSession()
{
	std::scoped_lock lock {_mutex};

	if (_freeSessions.empty())
	{
		if (_acquiredSessions.size() == _maxSessionCount)
			throw LmsException {"Too many database sessions!"};

		_freeSessions.emplace_back(std::make_unique<Session>(_db));
	}

	std::unique_ptr<Session> session {std::move(_freeSessions.back())};
	_freeSessions.pop_back();
	_acquiredSessions.push_back(std::move(session));

	return *_acquiredSessions.back().get();
}

void
SessionPool::releaseSession(Session& sessionToRelease)
{
	std::scoped_lock lock {_mutex};

	auto it {std::find_if(std::begin(_acquiredSessions), std::end(_acquiredSessions), [&](const std::unique_ptr<Session>& session) { return session.get() == &sessionToRelease; })};
	if (it == std::end(_acquiredSessions))
		throw LmsException {"Unknown released Session!"};

	std::unique_ptr<Session> session {std::move(*it)};
	_acquiredSessions.erase(it);
	_freeSessions.push_back(std::move(session));
}

} // namespace Database
