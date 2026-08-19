/*
 * Copyright (C) 2025 Emeric Poupon
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

#include <span>

#include <boost/asio/io_context.hpp>
#include <boost/asio/io_context_strand.hpp>
#include <boost/asio/steady_timer.hpp>

#include <Wt/WDateTime.h>

#include "database/objects/Types.hpp"

#include "services/scrobbling/Listen.hpp"

#include "Utils.hpp"

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

namespace lms::scrobbling::lastFm
{
    class ScrobblingsSynchronizer
    {
    public:
        ScrobblingsSynchronizer(boost::asio::io_context& ioContext, db::IDb& db, core::http::IClient& client);
        ~ScrobblingsSynchronizer();

        ScrobblingsSynchronizer(const ScrobblingsSynchronizer&) = delete;
        ScrobblingsSynchronizer& operator=(const ScrobblingsSynchronizer&) = delete;

        void enqueListen(const TimedListen& listen);
        void enqueListenNow(const Listen& listen);

    private:
        void enqueListen(const Listen& listen, const Wt::WDateTime& timePoint);
        bool saveListen(const TimedListen& listen, db::SyncState syncState);

        void enquePendingListens();
        void sendScrobbleBatch(const utils::LastFmCredentials& creds, std::span<const TimedListen> listens);
        void scheduleSubmit(std::chrono::seconds fromNow);

        boost::asio::io_context& _ioContext;
        boost::asio::io_context::strand _strand{ _ioContext };
        db::IDb& _db;
        std::chrono::hours _submitPeriod;
        boost::asio::steady_timer _submitTimer{ _ioContext };
        core::http::IClient& _client;
    };
} // namespace lms::scrobbling::lastFm
