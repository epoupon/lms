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

#include <mutex>
#include <string>
#include <vector>

#include <Wt/Dbo/SqlConnectionPool.h>

#include "core/RecursiveSharedMutex.hpp"

#include "database/IDb.hpp"

namespace lms::db
{
    class Db : public IDb
    {
    public:
        Db(const std::filesystem::path& dbPath, std::size_t connectionCount);

        void executeSql(const std::string& sql);

    private:
        Db(const Db&) = delete;
        Db& operator=(const Db&) = delete;

        friend class Session;

        Session& getTLSSession() override;

        core::RecursiveSharedMutex& getMutex() { return _sharedMutex; }
        Wt::Dbo::SqlConnectionPool& getConnectionPool() { return *_connectionPool; }

        void logPageSize();
        void logCacheSize();
        void logCompileOptions();
        void performQuickCheck();
        void performIntegrityCheck();
        void performForeignKeyConstraintsCheck();

        class ScopedConnection
        {
        public:
            ScopedConnection(Wt::Dbo::SqlConnectionPool& pool);
            ~ScopedConnection();

            Wt::Dbo::SqlConnection* operator->() const;
            Wt::Dbo::SqlConnection& operator*() const { return *_connection; }

        private:
            ScopedConnection(const ScopedConnection&) = delete;
            ScopedConnection& operator=(const ScopedConnection&) = delete;

            Wt::Dbo::SqlConnectionPool& _connectionPool;
            std::unique_ptr<Wt::Dbo::SqlConnection> _connection;
        };

        core::RecursiveSharedMutex _sharedMutex;
        std::unique_ptr<Wt::Dbo::SqlConnectionPool> _connectionPool;

        std::mutex _tlsSessionsMutex;
        std::vector<std::unique_ptr<Session>> _tlsSessions;
    };

} // namespace lms::db
