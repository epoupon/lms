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

#include "database/Db.hpp"
#include "database/Session.hpp"
#include "database/StarredArtist.hpp"
#include "database/StarredRelease.hpp"
#include "database/Track.hpp"
#include "utils/IConfig.hpp"
#include "utils/http/IClient.hpp"
#include "utils/ILogger.hpp"
#include "utils/Service.hpp"
#include "Utils.hpp"

namespace Feedback::ListenBrainz
{
    namespace details
    {
        template <typename StarredObjType>
        void onStarred(Database::Session& session, typename StarredObjType::IdType id)
        {
            auto transaction{ session.createWriteTransaction() };

            if (auto starredObj{ StarredObjType::find(session, id) })
            {
                // maybe in the future this will be supported by ListenBrainz so set it to PendingAdd for all types
                starredObj.modify()->setSyncState(Database::SyncState::PendingAdd);
            }
        }

        template <typename StarredObjType>
        void onUnstarred(Database::Session& session, typename StarredObjType::IdType id)
        {
            auto transaction{ session.createWriteTransaction() };

            if (auto starredObj{ StarredObjType::find(session, id) })
                starredObj.remove();
        }
    }

    ListenBrainzBackend::ListenBrainzBackend(boost::asio::io_context& ioContext, Database::Db& db)
        : _ioContext{ ioContext }
        , _db{ db }
        , _baseAPIUrl{ Service<IConfig>::get()->getString("listenbrainz-api-base-url", "https://api.listenbrainz.org") }
        , _client{ Http::createClient(_ioContext, _baseAPIUrl) }
        , _feedbacksSynchronizer{ _ioContext, db, *_client }
    {
        LOG(INFO, "Starting ListenBrainz feedback backend... API endpoint = '" << _baseAPIUrl << "'");
    }

    ListenBrainzBackend::~ListenBrainzBackend()
    {
        LOG(INFO, "Stopped ListenBrainz feedback backend!");
    }

    void ListenBrainzBackend::onStarred(Database::StarredArtistId starredArtistId)
    {
        details::onStarred<Database::StarredArtist>(_db.getTLSSession(), starredArtistId);
    }

    void ListenBrainzBackend::onUnstarred(Database::StarredArtistId starredArtistId)
    {
        details::onUnstarred<Database::StarredArtist>(_db.getTLSSession(), starredArtistId);
    }

    void ListenBrainzBackend::onStarred(Database::StarredReleaseId starredReleaseId)
    {
        details::onStarred<Database::StarredRelease>(_db.getTLSSession(), starredReleaseId);
    }

    void ListenBrainzBackend::onUnstarred(Database::StarredReleaseId starredReleaseId)
    {
        details::onUnstarred<Database::StarredRelease>(_db.getTLSSession(), starredReleaseId);
    }

    void ListenBrainzBackend::onStarred(Database::StarredTrackId starredTrackId)
    {
        _feedbacksSynchronizer.enqueFeedback(FeedbackType::Love, starredTrackId);
    }

    void ListenBrainzBackend::onUnstarred(Database::StarredTrackId starredtrackId)
    {
        _feedbacksSynchronizer.enqueFeedback(FeedbackType::Erase, starredtrackId);
    }
} // namespace Scrobbling::ListenBrainz
