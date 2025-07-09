/*
 * Copyright (C) 2023 Emeric Poupon
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

#include "ListenBrainzBackend.hpp"

#include "core/IConfig.hpp"
#include "core/Service.hpp"
#include "core/http/IClient.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/StarredArtist.hpp"
#include "database/objects/StarredRelease.hpp"
#include "database/objects/Track.hpp"

#include "Utils.hpp"

namespace lms::feedback::listenBrainz
{
    namespace details
    {
        template<typename StarredObjType>
        void onStarred(db::Session& session, typename StarredObjType::IdType id)
        {
            auto transaction{ session.createWriteTransaction() };

            if (auto starredObj{ StarredObjType::find(session, id) })
            {
                // maybe in the future this will be supported by ListenBrainz so set it to PendingAdd for all types
                starredObj.modify()->setSyncState(db::SyncState::PendingAdd);
            }
        }

        template<typename StarredObjType>
        void onUnstarred(db::Session& session, typename StarredObjType::IdType id)
        {
            auto transaction{ session.createWriteTransaction() };

            if (auto starredObj{ StarredObjType::find(session, id) })
                starredObj.remove();
        }
    } // namespace details

    ListenBrainzBackend::ListenBrainzBackend(boost::asio::io_context& ioContext, db::IDb& db)
        : _ioContext{ ioContext }
        , _db{ db }
        , _baseAPIUrl{ core::Service<core::IConfig>::get()->getString("listenbrainz-api-base-url", "https://api.listenbrainz.org") }
        , _client{ core::http::createClient(_ioContext, _baseAPIUrl) }
        , _feedbacksSynchronizer{ _ioContext, db, *_client }
    {
        LOG(INFO, "Starting ListenBrainz feedback backend... API endpoint = '" << _baseAPIUrl << "'");
    }

    ListenBrainzBackend::~ListenBrainzBackend()
    {
        LOG(INFO, "Stopped ListenBrainz feedback backend!");
    }

    void ListenBrainzBackend::onStarred(db::StarredArtistId starredArtistId)
    {
        details::onStarred<db::StarredArtist>(_db.getTLSSession(), starredArtistId);
    }

    void ListenBrainzBackend::onUnstarred(db::StarredArtistId starredArtistId)
    {
        details::onUnstarred<db::StarredArtist>(_db.getTLSSession(), starredArtistId);
    }

    void ListenBrainzBackend::onStarred(db::StarredReleaseId starredReleaseId)
    {
        details::onStarred<db::StarredRelease>(_db.getTLSSession(), starredReleaseId);
    }

    void ListenBrainzBackend::onUnstarred(db::StarredReleaseId starredReleaseId)
    {
        details::onUnstarred<db::StarredRelease>(_db.getTLSSession(), starredReleaseId);
    }

    void ListenBrainzBackend::onStarred(db::StarredTrackId starredTrackId)
    {
        _feedbacksSynchronizer.enqueFeedback(FeedbackType::Love, starredTrackId);
    }

    void ListenBrainzBackend::onUnstarred(db::StarredTrackId starredtrackId)
    {
        _feedbacksSynchronizer.enqueFeedback(FeedbackType::Erase, starredtrackId);
    }
} // namespace lms::feedback::listenBrainz
