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

#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include "Session.hpp"

namespace Database {

class SessionPool
{
	public:
		class ScopedSession
		{
			public:
				ScopedSession(SessionPool& pool) : _pool {pool}, _session {_pool.acquireSession()} {}
				~ScopedSession() { _pool.releaseSession(_session); }

				ScopedSession(const ScopedSession&) = delete;
				ScopedSession(ScopedSession&&) = delete;
				ScopedSession& operator=(const ScopedSession&) = delete;
				ScopedSession& operator=(ScopedSession&&) = delete;

				Session& get() { return _session; }

			private:
				SessionPool& _pool;
				Session& _session;
		};

		SessionPool(Db& database, std::size_t maxSessionCount = 30);

		SessionPool(const SessionPool&) = delete;
		SessionPool(SessionPool&&) = delete;
		SessionPool& operator=(const SessionPool&) = delete;
		SessionPool& operator=(SessionPool&&) = delete;

	private:
		friend class ScopedSession;
		Session& acquireSession();
		void releaseSession(Session& session);

		std::mutex	_mutex;
		Db&		_db;
		std::size_t	_maxSessionCount;
		std::vector<std::unique_ptr<Session>>	_freeSessions;
		std::vector<std::unique_ptr<Session>>	_acquiredSessions;
};

} // namespace Database


