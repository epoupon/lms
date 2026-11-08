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

#include <cassert>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <Wt/Dbo/Logger.h>
#include <Wt/Dbo/SqlConnection.h>
#include <Wt/Dbo/SqlConnectionPool.h>
#include <Wt/Dbo/SqlStatement.h>

#include "core/IConfig.hpp"
#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"
#include "core/Service.hpp"

#include "database/objects/ScanSettings.hpp"
#include "database/objects/ServerInfo.hpp"

#include "ConnectionPool.hpp"
#include "Db.hpp"
#include "Migration.hpp"
#include "Schema.hpp"
#include "Session.hpp"
#include "Utils.hpp"

namespace lms::db
{
    namespace
    {
        class ScopedConnection
        {
        public:
            ScopedConnection(Wt::Dbo::SqlConnectionPool& pool)
                : _connectionPool{ pool }
                , _connection{ _connectionPool.getConnection() }
            {
            }

            ~ScopedConnection()
            {
                _connectionPool.returnConnection(std::move(_connection));
            }

            ScopedConnection(const ScopedConnection&) = delete;
            ScopedConnection& operator=(const ScopedConnection&) = delete;

            Wt::Dbo::SqlConnection* operator->() const { return _connection.get(); }
            Wt::Dbo::SqlConnection& operator*() const { return *_connection; }

        private:
            Wt::Dbo::SqlConnectionPool& _connectionPool;
            std::unique_ptr<Wt::Dbo::SqlConnection> _connection;
        };

        template<typename T>
        std::optional<T> readPragma(Wt::Dbo::SqlConnection& connection, const std::string& pragma)
        {
            auto statement{ connection.prepareStatement("PRAGMA " + pragma) };
            statement->execute();

            T value{};
            if (statement->nextRow() && statement->getResult(0, &value))
                return value;

            return std::nullopt;
        }

        void logConnectionSettings(Wt::Dbo::SqlConnection& connection)
        {
            if (const auto pageSize{ readPragma<int>(connection, "page_size") })
                LMS_LOG(DB, INFO, "Page size set to " << *pageSize);

            if (const auto cacheSize{ readPragma<int>(connection, "cache_size") })
                LMS_LOG(DB, INFO, "Cache size set to " << *cacheSize);

            if (const auto mmapSize{ readPragma<long long>(connection, "mmap_size") })
                LMS_LOG(DB, INFO, "Mmap size set to " << *mmapSize);
        }

        void logCompileOptions(Wt::Dbo::SqlConnection& connection)
        {
            LMS_LOG(DB, INFO, "Sqlite3 compile options:");

            auto statement{ connection.prepareStatement("PRAGMA compile_options") };
            statement->execute();

            while (statement->nextRow())
            {
                std::string compileOption;
                if (statement->getResult(0, &compileOption, static_cast<int>(compileOption.capacity())))
                    LMS_LOG(DB, INFO, compileOption);
            }
        }

        void checkIntegrity(Wt::Dbo::SqlConnection& connection, const std::string& pragma, std::string_view checkName)
        {
            LMS_LOG(DB, INFO, checkName << "...");

            auto statement{ connection.prepareStatement("PRAGMA " + pragma) };
            statement->execute();

            bool passed{};
            while (statement->nextRow())
            {
                std::string result;
                statement->getResult(0, &result, static_cast<int>(result.capacity()));

                if (result == "ok")
                {
                    passed = true;
                    break;
                }

                LMS_LOG(DB, ERROR, checkName << " error: " << result);
            }

            if (passed)
                LMS_LOG(DB, INFO, checkName << " passed!");
            else
                LMS_LOG(DB, ERROR, checkName << " done with errors!");
        }

        void checkForeignKeyConstraints(Wt::Dbo::SqlConnection& connection)
        {
            LMS_LOG(DB, INFO, "Checking foreign key constraints...");

            auto statement{ connection.prepareStatement("PRAGMA foreign_key_check") };
            statement->execute();

            bool passed{ true };
            // see https://www.sqlite.org/pragma.html#pragma_foreign_key_check for expected result
            while (statement->nextRow())
            {
                passed = false;

                std::string table;
                long long rowId{};
                std::string referredTable;

                statement->getResult(0, &table, static_cast<int>(table.capacity()));
                statement->getResult(1, &rowId);
                statement->getResult(2, &referredTable, static_cast<int>(referredTable.capacity()));

                LMS_LOG(DB, ERROR, "Foreign key constraint failed in table '" << table << "', rowid = " << rowId << ", referred table = '" << referredTable << "'");
            }

            if (!passed)
                throw Exception{ "Foreign key constraints check failed! Please restore from a backup or recreate the database." };

            LMS_LOG(DB, INFO, "Foreign key constraints check passed!");
        }

        void createScanSettingsIfNeeded(Session& session, RecommendationEngineType defaultRecommendationEngineType)
        {
            auto transaction{ session.createWriteTransaction() };

            if (!ScanSettings::find(session))
                ScanSettings::create(session).modify()->setRecommendationEngineType(defaultRecommendationEngineType);
        }

        void createServerInfoIfNeeded(Session& session)
        {
            auto transaction{ session.createWriteTransaction() };

            ServerInfo::getOrCreate(session);
        }

        bool isVacuumNeeded(Session& session)
        {
            long pageCount{};
            long freeListCount{};

            {
                auto transaction{ session.createReadTransaction() };
                pageCount = utils::fetchQuerySingleResult(session.getDboSession()->query<long>("SELECT page_count FROM pragma_page_count"));
                freeListCount = utils::fetchQuerySingleResult(session.getDboSession()->query<long>("SELECT freelist_count FROM pragma_freelist_count"));
            }

            LMS_LOG(DB, INFO, "page stats: page_count = " << pageCount << ", freelist_count = " << freeListCount);
            return freeListCount >= (pageCount / 10);
        }

        std::vector<std::string> retrieveEntriesToAnalyze(Session& session)
        {
            auto transaction{ session.createReadTransaction() };
            return utils::fetchQueryResults(session.getDboSession()->query<std::string>("SELECT name FROM sqlite_master WHERE type='table' OR type ='index'"));
        }

        void analyzeEntry(Session& session, const std::string& entry)
        {
            LMS_LOG(DB, DEBUG, "Analyzing " << entry);
            {
                auto transaction{ session.createWriteTransaction() };
                utils::executeCommand(*session.getDboSession(), "ANALYZE " + entry);
            }
            LMS_LOG(DB, DEBUG, "Analyzing " << entry << ": done!");
        }
    } // namespace

    std::unique_ptr<IDb> createDb(const std::filesystem::path& dbPath)
    {
        return std::make_unique<Db>(dbPath);
    }

    Db::Db(const std::filesystem::path& dbPath)
    {
        Wt::Dbo::logToWt();

        bool showQueries{};
        std::string checkType{ "quick" };
        LMS_LOG(DB, INFO, "Creating connection pool on file " << dbPath);

        if (core::IConfig * config{ core::Service<core::IConfig>::get() }) // may not be here on testU
        {
            showQueries = config->getBool("db-show-queries", false);
            checkType = config->getString("db-integrity-check", "quick");
        }

        _connectionPool = std::make_unique<ConnectionPool>(dbPath, showQueries);

        ScopedConnection connection{ *_connectionPool };

        logConnectionSettings(*connection);
        logCompileOptions(*connection);

        if (checkType == "quick")
        {
            checkIntegrity(*connection, "quick_check", "Quick database check");
        }
        else if (checkType == "full")
        {
            checkIntegrity(*connection, "integrity_check", "Database integrity check");
            checkForeignKeyConstraints(*connection);
        }
        else if (checkType != "none")
        {
            throw Exception{ "Invalid 'db-integrity-check' value: '" + checkType + "'. Expected 'quick', 'full' or 'none'." };
        }
    }

    Db::~Db() = default;

    void Db::prepare(RecommendationEngineType defaultRecommendationEngineType)
    {
        Session session{ *this };

        Schema::createTablesIfNeeded(session);
        const bool migrationPerformed{ Migration::doDbMigration(session) };
        createScanSettingsIfNeeded(session, defaultRecommendationEngineType);
        createServerInfoIfNeeded(session);
        Schema::createIndexesIfNeeded(session);

        // As this may be quite long, we only do it during startup
        if (migrationPerformed || isVacuumNeeded(session))
            vacuum();
    }

    void Db::analyze(const ProgressCallback& onProgress, const ShouldAbortCallback& shouldAbort)
    {
        LMS_SCOPED_TRACE_OVERVIEW("Database", "Analyze");
        LMS_LOG(DB, INFO, "Performing database analyze... This may take a while...");

        Session session{ *this };

        // first select all the tables and indexes, and then analyze one by one in order to not have a big lock
        const std::vector<std::string> entries{ retrieveEntriesToAnalyze(session) };

        std::size_t processedEntries{};
        if (onProgress)
            onProgress(processedEntries, entries.size());

        for (const std::string& entry : entries)
        {
            if (shouldAbort && shouldAbort())
                break;

            analyzeEntry(session, entry);

            processedEntries++;
            if (onProgress)
                onProgress(processedEntries, entries.size());
        }

        // Also on abort: the entries analyzed so far are committed
        _connectionPool->requestConnectionsReload();

        LMS_LOG(DB, INFO, "Analyze " << (processedEntries == entries.size() ? "complete" : "aborted") << "!");
    }

    void Db::vacuum()
    {
        LMS_SCOPED_TRACE_OVERVIEW("Database", "Vacuum");
        LMS_LOG(DB, INFO, "Performing vacuum... This may take a while...");

        // We manually take a lock here since vacuum cannot be inside a transaction.
        {
            std::unique_lock lock{ _sharedMutex };
            executeOutsideTransaction("VACUUM");
        }

        LMS_LOG(DB, INFO, "Vacuum complete!");
    }

    void Db::executeOutsideTransaction(const std::string& sql)
    {
        ScopedConnection connection{ *_connectionPool };
        connection->executeSql(sql);
    }

    std::unique_ptr<ISession> Db::createSession()
    {
        return std::make_unique<Session>(*this);
    }

    ISession& Db::getTLSSession()
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

        // For now, multiple databases are not handled
        assert(&tlsSession->getDb() == this);

        return *tlsSession;
    }
} // namespace lms::db
