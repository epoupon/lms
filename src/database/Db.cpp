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

#include "Db.hpp"

#include <Wt/Dbo/FixedSqlConnectionPool.h>
#include <Wt/Dbo/backend/Sqlite3.h>

#include "utils/Logger.hpp"
#include "User.hpp"

namespace Database {

// Session living class handling the database and the login
Db::Db(const std::filesystem::path& dbPath)
{
	LMS_LOG(DB, INFO) << "Creating connection pool on file " << dbPath.string();

	std::unique_ptr<Wt::Dbo::backend::Sqlite3> connection {std::make_unique<Wt::Dbo::backend::Sqlite3>(dbPath.string())};
	connection->executeSql("pragma journal_mode=WAL");
//	connection->setProperty("show-queries", "true");

	auto connectionPool = std::make_unique<Wt::Dbo::FixedSqlConnectionPool>(std::move(connection), 10);
	connectionPool->setTimeout(std::chrono::seconds(10));

	_connectionPool = std::move(connectionPool);

	{
		auto session {createSession()};
		session->prepareTables();
	}

}

std::unique_ptr<Session>
Db::createSession()
{
	return std::unique_ptr<Session>(new Session {_sharedMutex, *_connectionPool.get()});
}

} // namespace Database


