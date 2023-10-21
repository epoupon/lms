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

#include "services/database/Db.hpp"

#include <Wt/Dbo/FixedSqlConnectionPool.h>
#include <Wt/Dbo/backend/Sqlite3.h>

#include "services/database/Session.hpp"
#include "services/database/User.hpp"
#include "utils/Logger.hpp"

namespace Database {

    // Session living class handling the database and the login
    Db::Db(const std::filesystem::path& dbPath, std::size_t connectionCount)
    {
        LMS_LOG(DB, INFO) << "Creating connection pool on file " << dbPath.string();

        auto connection{ std::make_unique<Wt::Dbo::backend::Sqlite3>(dbPath.string()) };
        //	connection->setProperty("show-queries", "true");
        connection->executeSql("pragma journal_mode=WAL");
        connection->executeSql("pragma synchronous=normal");
        connection->executeSql("pragma analysis_limit=1000"); // to help make analyze command faster

        auto connectionPool{ std::make_unique<Wt::Dbo::FixedSqlConnectionPool>(std::move(connection), connectionCount) };
        connectionPool->setTimeout(std::chrono::seconds{ 10 });

        _connectionPool = std::move(connectionPool);
    }

    void Db::executeSql(const std::string& sql)
    {
        ScopedConnection connection{ *_connectionPool };
        connection->executeSql(sql);
    }

    Session& Db::getTLSSession()
    {
        static thread_local Session* tlsSession{};

        if (!tlsSession)
        {
            auto newSession{ std::make_unique<Session>(*this) };
            tlsSession = newSession.get();

            {
                std::scoped_lock lock{ _tlsSessionsMutex };
                _tlsSessions.push_back(std::move(newSession));
            }
        }

        return *tlsSession;
    }

    Db::ScopedConnection::ScopedConnection(Wt::Dbo::SqlConnectionPool& pool)
        : _connectionPool{ pool }
        , _connection{ _connectionPool.getConnection() }
    {
    }

    Db::ScopedConnection::~ScopedConnection()
    {
        _connectionPool.returnConnection(std::move(_connection));
    }

    Wt::Dbo::SqlConnection* Db::ScopedConnection::operator->() const
    {
        return _connection.get();
    }

} // namespace Database


