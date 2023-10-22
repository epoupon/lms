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

#pragma once

#include <filesystem>

#include <Wt/Dbo/SqlConnectionPool.h>

#include "utils/RecursiveSharedMutex.hpp"

namespace Database {

    class Session;
    class Db
    {
    public:
        Db(const std::filesystem::path& dbPath, std::size_t connectionCount = 10);

        Session& getTLSSession();

        void executeSql(const std::string& sql);

    private:
        Db(const Db&) = delete;
        Db& operator=(const Db&) = delete;

        friend class Session;

        RecursiveSharedMutex& getMutex() { return _sharedMutex; }
        Wt::Dbo::SqlConnectionPool& getConnectionPool() { return *_connectionPool; }

        class ScopedConnection
        {
        public:
            ScopedConnection(Wt::Dbo::SqlConnectionPool& pool);
            ~ScopedConnection();

            Wt::Dbo::SqlConnection* operator->() const;

        private:
            ScopedConnection(const ScopedConnection&) = delete;
            ScopedConnection& operator=(const ScopedConnection&) = delete;

            Wt::Dbo::SqlConnectionPool& _connectionPool;
            std::unique_ptr<Wt::Dbo::SqlConnection> _connection;
        };

        RecursiveSharedMutex				_sharedMutex;
        std::unique_ptr<Wt::Dbo::SqlConnectionPool>	_connectionPool;

        std::mutex _tlsSessionsMutex;
        std::vector<std::unique_ptr<Session>> _tlsSessions;
    };

} // namespace Database


