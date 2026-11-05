/*
 * Copyright (C) 2026 Emeric Poupon
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

#include "ConnectionPool.hpp"

#include <cassert>

#include <Wt/Dbo/backend/Sqlite3.h>

#include "core/ILogger.hpp"

namespace lms::db
{
    namespace
    {
        class Connection : public Wt::Dbo::backend::Sqlite3
        {
        public:
            Connection(const std::filesystem::path& dbPath, bool showQueries, std::size_t version)
                : Wt::Dbo::backend::Sqlite3{ dbPath.string() }
                , _version{ version }
            {
                setProperty("show-queries", showQueries ? "true" : "false");

                LMS_LOG(DB, DEBUG, "Setting per-connection settings...");
                executeSql("PRAGMA journal_mode=WAL");
                executeSql("PRAGMA synchronous=normal");
                executeSql("PRAGMA temp_store=MEMORY");
                executeSql("PRAGMA automatic_index=0");
                LMS_LOG(DB, DEBUG, "Setting per-connection settings done!");
            }
            ~Connection() override = default;
            Connection(const Connection&) = delete;
            Connection& operator=(const Connection&) = delete;

            std::size_t getVersion() const { return _version; }

        private:
            const std::size_t _version;
        };
    } // namespace

    ConnectionPool::ConnectionPool(const std::filesystem::path& dbPath, bool showQueries)
        : _dbPath{ dbPath }
        , _showQueries{ showQueries }
    {
    }

    ConnectionPool::~ConnectionPool() = default;

    void ConnectionPool::requestConnectionsReload()
    {
        _version++;
    }

    std::unique_ptr<Wt::Dbo::SqlConnection> ConnectionPool::getConnection()
    {
        std::unique_ptr<Wt::Dbo::SqlConnection> connection;
        {
            const std::scoped_lock lock{ _mutex };
            
            if (!_freeConnections.empty())
            {
                connection = std::move(_freeConnections.back());
                _freeConnections.pop_back();
            }
        }

        const std::size_t version{ _version };
        if (!connection || static_cast<const Connection&>(*connection).getVersion() != version)
        {
            // Opened before closing the old one: closing the last connection on a database would checkpoint and remove the WAL
            connection = std::make_unique<Connection>(_dbPath, _showQueries, version);
        }

        return connection;
    }

    void ConnectionPool::returnConnection(std::unique_ptr<Wt::Dbo::SqlConnection> connection)
    {
        const std::scoped_lock lock{ _mutex };

        _freeConnections.push_back(std::move(connection));
    }

    void ConnectionPool::prepareForDropTables() const
    {
        // Only called by Wt::Dbo::Session::dropTables, never used
        assert(false);
    }
} // namespace lms::db
