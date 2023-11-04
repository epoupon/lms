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

#include "services/database/Db.hpp"
#include "services/database/Session.hpp"
#include "services/database/StarredArtist.hpp"
#include "services/database/StarredRelease.hpp"
#include "services/database/StarredTrack.hpp"

namespace Feedback
{
    namespace details
    {
        template <typename StarredObjType>
        void onStarred(Database::Session& session, typename StarredObjType::IdType id)
        {
            auto transaction{ session.createUniqueTransaction() };

            if (auto starredObj{ StarredObjType::find(session, id) })
                starredObj.modify()->setSyncState(Database::SyncState::Synchronized);
        }

        template <typename StarredObjType>
        void onUnstarred(Database::Session& session, typename StarredObjType::IdType id)
        {
            auto transaction{ session.createUniqueTransaction() };

            if (auto starredObj{ StarredObjType::find(session, id) })
                starredObj.remove();
        }
    }

    InternalBackend::InternalBackend(Database::Db& db)
        : _db{ db }
    {}

    void InternalBackend::onStarred(Database::StarredArtistId starredArtistId)
    {
        details::onStarred<Database::StarredArtist>(_db.getTLSSession(), starredArtistId);
    }

    void InternalBackend::onUnstarred(Database::StarredArtistId starredArtistId)
    {
        details::onUnstarred<Database::StarredArtist>(_db.getTLSSession(), starredArtistId);
    }

    void InternalBackend::onStarred(Database::StarredReleaseId starredReleaseId)
    {
        details::onStarred<Database::StarredRelease>(_db.getTLSSession(), starredReleaseId);
    }

    void InternalBackend::onUnstarred(Database::StarredReleaseId starredReleaseId)
    {
        details::onUnstarred<Database::StarredRelease>(_db.getTLSSession(), starredReleaseId);
    }

    void InternalBackend::onStarred(Database::StarredTrackId starredTrackId)
    {
        details::onStarred<Database::StarredTrack>(_db.getTLSSession(), starredTrackId);
    }

    void InternalBackend::onUnstarred(Database::StarredTrackId starredTrackId)
    {
        details::onUnstarred<Database::StarredTrack>(_db.getTLSSession(), starredTrackId);
    }
} // Feedback
