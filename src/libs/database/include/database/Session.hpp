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

#include <Wt/Dbo/Dbo.h>
#include <Wt/Dbo/SqlConnectionPool.h>

#include <string>
#include <vector>
#include "core/ITraceLogger.hpp"
#include "core/RecursiveSharedMutex.hpp"
#include "database/Object.hpp"
#include "database/TransactionChecker.hpp"

namespace lms::db
{
    class WriteTransaction
    {
    public:
        ~WriteTransaction();

    private:
        friend class Session;
        WriteTransaction(core::RecursiveSharedMutex& mutex, Wt::Dbo::Session& session);

        WriteTransaction(const WriteTransaction&) = delete;
        WriteTransaction& operator=(const WriteTransaction&) = delete;

        const std::unique_lock<core::RecursiveSharedMutex> _lock;
        const core::tracing::ScopedTrace _trace{ "Database", core::tracing::Level::Detailed, "WriteTransaction" }; // before actual transaction
        Wt::Dbo::Transaction _transaction;
    };

    class ReadTransaction
    {
    public:
        ~ReadTransaction();

    private:
        friend class Session;
        ReadTransaction(Wt::Dbo::Session& session);

        ReadTransaction(const ReadTransaction&) = delete;
        ReadTransaction& operator=(const ReadTransaction&) = delete;

        const core::tracing::ScopedTrace _trace{ "Database", core::tracing::Level::Detailed, "ReadTransaction" }; // before actual transaction
        Wt::Dbo::Transaction _transaction;
    };

    class Db;
    class Session
    {
    public:
        Session(Db& database);

        [[nodiscard]] WriteTransaction createWriteTransaction();
        [[nodiscard]] ReadTransaction createReadTransaction();

        void checkWriteTransaction()
        {
#if LMS_CHECK_TRANSACTION_ACCESSES
            TransactionChecker::checkWriteTransaction(_session);
#endif
        }
        void checkReadTransaction()
        {
#if LMS_CHECK_TRANSACTION_ACCESSES
            TransactionChecker::checkReadTransaction(_session);
#endif
        }

        void execute(std::string_view statement);

        // All these methods will acquire transactions
        void fullAnalyze(); // helper for retrieveEntriesToAnalyze + analyzeEntry
        void retrieveEntriesToAnalyze(std::vector<std::string>& entryList);
        void analyzeEntry(const std::string& entry);

        void prepareTablesIfNeeded(); // need to run only once at startup
        bool migrateSchemaIfNeeded(); // returns true if migration was performed
        void createIndexesIfNeeded();
        void vacuumIfNeeded();
        void vacuum();
        void refreshTracingLoggerStats();

        // returning a ptr here to ease further wrapping using operator->
        Wt::Dbo::Session* getDboSession() { return &_session; }
        Db& getDb() { return _db; }

        template <typename Object, typename... Args>
        typename Object::pointer create(Args&&... args)
        {
            checkWriteTransaction();

            typename Object::pointer res{ Object::create(*this, std::forward<Args>(args)...) };
            getDboSession()->flush();

            if (res->hasOnPostCreated())
                res.modify()->onPostCreated();

            return res;
        }

    private:
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;

        Db& _db;
        Wt::Dbo::Session	_session;
    };
} // namespace lms::db
