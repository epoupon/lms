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

#pragma once

#include <atomic>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <mutex>
#include <vector>

#include <Wt/Dbo/SqlConnectionPool.h>

namespace lms::db
{
    class ConnectionPool : public Wt::Dbo::SqlConnectionPool
    {
    public:
        ConnectionPool(const std::filesystem::path& dbPath, bool showQueries);
        ~ConnectionPool() override;
        ConnectionPool(const ConnectionPool&) = delete;
        ConnectionPool& operator=(const ConnectionPool&) = delete;

        void requestConnectionsReload();

    private:
        std::unique_ptr<Wt::Dbo::SqlConnection> getConnection() override;
        void returnConnection(std::unique_ptr<Wt::Dbo::SqlConnection> connection) override;
        void prepareForDropTables() const override;

        const std::filesystem::path _dbPath;
        const bool _showQueries;
        std::atomic<std::size_t> _version;

        std::mutex _mutex;
        std::vector<std::unique_ptr<Wt::Dbo::SqlConnection>> _freeConnections;
    };
} // namespace lms::db
