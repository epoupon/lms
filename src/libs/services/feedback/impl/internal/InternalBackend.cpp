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
#include "database/Session.hpp"
#include "database/StarredArtist.hpp"
#include "database/StarredRelease.hpp"
#include "database/StarredTrack.hpp"

namespace lms::feedback
{
    namespace details
    {
        template<typename StarredObjType>
        void onStarred(db::Session& session, typename StarredObjType::IdType id)
        {
            auto transaction{ session.createWriteTransaction() };

            if (auto starredObj{ StarredObjType::find(session, id) })
                starredObj.modify()->setSyncState(db::SyncState::Synchronized);
        }

        template<typename StarredObjType>
        void onUnstarred(db::Session& session, typename StarredObjType::IdType id)
        {
            auto transaction{ session.createWriteTransaction() };

            if (auto starredObj{ StarredObjType::find(session, id) })
                starredObj.remove();
        }
    } // namespace details

    InternalBackend::InternalBackend(db::Db& db)
        : _db{ db }
    {
    }

    void InternalBackend::onStarred(db::StarredArtistId starredArtistId)
    {
        details::onStarred<db::StarredArtist>(_db.getTLSSession(), starredArtistId);
    }

    void InternalBackend::onUnstarred(db::StarredArtistId starredArtistId)
    {
        details::onUnstarred<db::StarredArtist>(_db.getTLSSession(), starredArtistId);
    }

    void InternalBackend::onStarred(db::StarredReleaseId starredReleaseId)
    {
        details::onStarred<db::StarredRelease>(_db.getTLSSession(), starredReleaseId);
    }

    void InternalBackend::onUnstarred(db::StarredReleaseId starredReleaseId)
    {
        details::onUnstarred<db::StarredRelease>(_db.getTLSSession(), starredReleaseId);
    }

    void InternalBackend::onStarred(db::StarredTrackId starredTrackId)
    {
        details::onStarred<db::StarredTrack>(_db.getTLSSession(), starredTrackId);
    }

    void InternalBackend::onUnstarred(db::StarredTrackId starredTrackId)
    {
        details::onUnstarred<db::StarredTrack>(_db.getTLSSession(), starredTrackId);
    }
} // namespace lms::feedback
