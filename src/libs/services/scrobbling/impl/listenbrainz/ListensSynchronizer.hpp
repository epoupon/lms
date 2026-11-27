/*
 * Copyright (C) 2021 Emeric Poupon
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

#include <optional>
#include <unordered_map>

#include <boost/asio/io_context.hpp>
#include <boost/asio/io_context_strand.hpp>
#include <boost/asio/steady_timer.hpp>

#include "core/LiteralString.hpp"
#include "database/objects/ListenBackendSyncId.hpp"
#include "database/objects/Types.hpp"
#include "database/objects/UserId.hpp"

#include "services/scrobbling/Listen.hpp"

namespace lms
{
    namespace core::http
    {
        class IClient;
    }
    namespace db
    {
        class IDb;
    }
} // namespace lms

namespace lms::scrobbling::listenBrainz
{
    class ListensSynchronizer
    {
    public:
        ListensSynchronizer(boost::asio::io_context& ioContext, db::IDb& db, core::http::IClient& client);
        ~ListensSynchronizer();

        ListensSynchronizer(const ListensSynchronizer&) = delete;
        ListensSynchronizer& operator=(const ListensSynchronizer&) = delete;

        void enqueListen(const TimedListen& listen);
        void enqueListenNow(const Listen& listen);
        void requestImmediateImport(db::UserId userId);
        void requestImmediateExport(db::UserId userId);

    private:
        bool saveListen(const TimedListen& listen, db::SyncState scrobblingState);
        void dropListen(db::ListenBackendSyncId syncId);
        // returns false if the listen was already synchronized or no longer waiting to be sent
        bool markListenSynchronized(db::ListenBackendSyncId syncId);

        void requestExportForEnabledUsers();

        struct UserContext
        {
            UserContext(db::UserId id, boost::asio::io_context& ioContext)
                : userId{ id }
                , exportState{ ioContext } {}
            ~UserContext() = default;

            UserContext(const UserContext&) = delete;
            UserContext& operator=(const UserContext&) = delete;

            const db::UserId userId;

            // Shared between the import (fetch from ListenBrainz) and submit (send to ListenBrainz) paths:
            // cached total listen count on ListenBrainz for this user, kept roughly in sync by both.
            std::optional<std::size_t> listenCount{};

            // Import-only bookkeeping, reset at the start of each import cycle.
            struct ImportState
            {
                bool importing{};
                std::string listenBrainzUserName; // need to be resolved first
                Wt::WDateTime maxDateTime;
                std::size_t fetchedListenCount{};
                std::size_t matchedListenCount{};
                std::size_t importedListenCount{};
                std::optional<std::size_t> pendingListenCount{}; // only committed to listenCount once the fetch actually completes
            };
            ImportState import;

            struct ExportState
            {
                ExportState(boost::asio::io_context& ioContext)
                    : retryTimer{ ioContext } {}

                bool exporting{}; // sending or waiting to retry
                std::size_t consecutiveFailures{};
                boost::asio::steady_timer retryTimer;
            };
            ExportState exportState;
        };

        UserContext& getUserContext(db::UserId userId);
        void startExport(UserContext& context);
        void sendNextListen(UserContext& context);
        void scheduleExportRetry(UserContext& context);
        void startImport(UserContext& context);
        void onImportEnded(UserContext& context);
        void onImportFailed(UserContext& context, core::LiteralString failedRequest, std::optional<int> httpStatus);
        void enqueValidateToken(UserContext& context);
        void enqueGetListenCount(UserContext& context);
        void enqueGetListens(UserContext& context);
        void processGetListensResponse(std::string_view body, UserContext& context);

        boost::asio::io_context& _ioContext;
        boost::asio::io_context::strand _strand{ _ioContext };
        db::IDb& _db;
        core::http::IClient& _client;

        std::unordered_map<db::UserId, UserContext> _userContexts;

        const std::size_t _maxSyncListenCount;
    };
} // namespace lms::scrobbling::listenBrainz
