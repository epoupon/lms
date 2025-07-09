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

#include "database/Types.hpp"
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

        void enqueListen(const TimedListen& listen);
        void enqueListenNow(const Listen& listen);

    private:
        void enqueListen(const Listen& listen, const Wt::WDateTime& timePoint);
        bool saveListen(const TimedListen& listen, db::SyncState scrobblinState);

        void enquePendingListens();

        struct UserContext
        {
            UserContext(db::UserId id)
                : userId{ id } {}

            UserContext(const UserContext&) = delete;
            UserContext(UserContext&&) = delete;
            UserContext& operator=(const UserContext&) = delete;
            UserContext& operator=(UserContext&&) = delete;

            const db::UserId userId;
            bool syncing{};
            std::optional<std::size_t> listenCount{};

            // resetted at each sync
            std::string listenBrainzUserName; // need to be resolved first
            Wt::WDateTime maxDateTime;
            std::size_t fetchedListenCount{};
            std::size_t matchedListenCount{};
            std::size_t importedListenCount{};
        };

        UserContext& getUserContext(db::UserId userId);
        bool isSyncing() const;
        void scheduleSync(std::chrono::seconds fromNow);
        void startSync();
        void startSync(UserContext& context);
        void onSyncEnded(UserContext& context);
        void enqueValidateToken(UserContext& context);
        void enqueGetListenCount(UserContext& context);
        void enqueGetListens(UserContext& context);
        void processGetListensResponse(std::string_view body, UserContext& context);

        boost::asio::io_context& _ioContext;
        boost::asio::io_context::strand _strand{ _ioContext };
        db::IDb& _db;
        boost::asio::steady_timer _syncTimer{ _ioContext };
        core::http::IClient& _client;

        std::unordered_map<db::UserId, UserContext> _userContexts;

        const std::size_t _maxSyncListenCount;
        const std::chrono::hours _syncListensPeriod;
    };
} // namespace lms::scrobbling::listenBrainz
