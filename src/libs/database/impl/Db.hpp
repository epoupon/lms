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

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "core/RecursiveSharedMutex.hpp"

#include "database/IDb.hpp"

namespace lms::db
{
    class ConnectionPool;
    class Session;

    class Db : public IDb
    {
    public:
        Db(const std::filesystem::path& dbPath);
        ~Db() override;

        void executeOutsideTransaction(const std::string& sql);

    private:
        Db(const Db&) = delete;
        Db& operator=(const Db&) = delete;

        friend class Session;

        ISession& getTLSSession() override;
        std::unique_ptr<ISession> createSession() override;
        void prepare(RecommendationEngineType defaultRecommendationEngineType) override;
        void analyze(const ProgressCallback& onProgress, const ShouldAbortCallback& shouldAbort) override;
        void vacuum() override;

        core::RecursiveSharedMutex& getMutex() { return _sharedMutex; }
        ConnectionPool& getConnectionPool() { return *_connectionPool; }

        core::RecursiveSharedMutex _sharedMutex;
        std::unique_ptr<ConnectionPool> _connectionPool;

        std::mutex _tlsSessionsMutex;
        std::vector<std::unique_ptr<Session>> _tlsSessions;
    };

} // namespace lms::db
