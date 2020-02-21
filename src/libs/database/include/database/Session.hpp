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

#include <mutex>
#include <memory>
#include <shared_mutex>

#include <Wt/Dbo/Dbo.h>
#include <Wt/Dbo/SqlConnectionPool.h>

namespace Database {

class UniqueTransaction
{
	public:
		~UniqueTransaction();

	private:
		friend class Session;
		UniqueTransaction(std::shared_mutex& mutex, Wt::Dbo::Session& session);

		std::unique_lock<std::shared_mutex> _lock;
		Wt::Dbo::Transaction _transaction;
};

class SharedTransaction
{
	public:
		~SharedTransaction();

	private:
		friend class Session;
		SharedTransaction(std::shared_mutex& mutex, Wt::Dbo::Session& session);

		std::shared_lock<std::shared_mutex> _lock;
		Wt::Dbo::Transaction _transaction;
};

class Db;
class Session
{
	public:
		Session (Db& database);

		Session(const Session&) = delete;
		Session(Session&&) = delete;
		Session& operator=(const Session&) = delete;
		Session& operator=(Session&&) = delete;

		[[nodiscard]] UniqueTransaction createUniqueTransaction();
		[[nodiscard]] SharedTransaction createSharedTransaction();

		void checkUniqueLocked();
		void checkSharedLocked();

		void optimize();

		void prepareTables(); // need to run only once at startup

		Wt::Dbo::Session& getDboSession() { return _session; }

	private:
		Session(std::shared_mutex& mutex, Wt::Dbo::SqlConnectionPool& connectionPool);

		void doDatabaseMigrationIfNeeded();

		Db&			_db;
		Wt::Dbo::Session	_session;
};

} // namespace Database


