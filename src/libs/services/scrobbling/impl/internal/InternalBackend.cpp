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

#include "InternalBackend.hpp"

#include "database/Db.hpp"
#include "database/Listen.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "database/User.hpp"

namespace Scrobbling
{
    InternalBackend::InternalBackend(Database::Db& db)
        : _db{ db }
    {}

    void InternalBackend::listenStarted(const Listen&)
    {
        // nothing to do
    }

    void InternalBackend::listenFinished(const Listen& listen, std::optional<std::chrono::seconds> duration)
    {
        // only record tracks that have been played for at least of few seconds...
        if (duration && *duration < std::chrono::seconds{ 5 })
            return;

        addTimedListen({ listen, Wt::WDateTime::currentDateTime() });
    }

    void InternalBackend::addTimedListen(const TimedListen& listen)
    {
        Database::Session& session{ _db.getTLSSession() };
        auto transaction{ session.createWriteTransaction() };

        if (Database::Listen::find(session, listen.userId, listen.trackId, Database::ScrobblingBackend::Internal, listen.listenedAt))
            return;

        const Database::User::pointer user{ Database::User::find(session, listen.userId) };
        if (!user)
            return;

        const Database::Track::pointer track{ Database::Track::find(session, listen.trackId) };
        if (!track)
            return;

        auto dbListen{ session.create<Database::Listen>(user, track, Database::ScrobblingBackend::Internal, listen.listenedAt) };
        dbListen.modify()->setSyncState(Database::SyncState::Synchronized);
    }
} // Scrobbling

