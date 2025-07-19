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

#include "database/objects/StarredTrackId.hpp"
#include "database/objects/UserId.hpp"

#include "FeedbackTypes.hpp"

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

namespace lms::feedback::listenBrainz
{
    class FeedbacksSynchronizer
    {
    public:
        FeedbacksSynchronizer(boost::asio::io_context& ioContext, db::IDb& db, core::http::IClient& client);
        ~FeedbacksSynchronizer() = default;
        FeedbacksSynchronizer(const FeedbacksSynchronizer&) = delete;
        FeedbacksSynchronizer& operator=(const FeedbacksSynchronizer&) = delete;

        void enqueFeedback(FeedbackType type, db::StarredTrackId starredTrackId);

    private:
        void onFeedbackSent(FeedbackType type, db::StarredTrackId starredTrackId);
        void enquePendingFeedbacks();

        struct UserContext
        {
            UserContext(db::UserId id)
                : userId{ id } {}

            ~UserContext() = default;
            UserContext(const UserContext&) = delete;
            UserContext& operator=(const UserContext&) = delete;

            const db::UserId userId;
            bool syncing{};
            std::optional<std::size_t> feedbackCount;

            // resetted at each sync
            std::string listenBrainzUserName; // need to be resolved first

            std::size_t currentOffset{};
            std::size_t fetchedFeedbackCount{};
            std::size_t matchedFeedbackCount{};
            std::size_t importedFeedbackCount{};
        };

        UserContext& getUserContext(db::UserId userId);
        bool isSyncing() const;
        void scheduleSync(std::chrono::seconds fromNow);
        void startSync();
        void startSync(UserContext& context);
        void onSyncEnded(UserContext& context);
        void enqueValidateToken(UserContext& context);
        void enqueGetFeedbackCount(UserContext& context);
        void enqueGetFeedbacks(UserContext& context);
        std::size_t processGetFeedbacks(std::string_view body, UserContext& context);
        void tryImportFeedback(const Feedback& feedback, UserContext& context);

        boost::asio::io_context& _ioContext;
        boost::asio::io_context::strand _strand{ _ioContext };
        db::IDb& _db;
        boost::asio::steady_timer _syncTimer{ _ioContext };
        core::http::IClient& _client;

        std::unordered_map<db::UserId, UserContext> _userContexts;

        const std::size_t _maxSyncFeedbackCount;
        const std::chrono::hours _syncFeedbacksPeriod;
    };
} // namespace lms::feedback::listenBrainz
