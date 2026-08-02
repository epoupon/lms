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
#include "database/objects/ArtistFeedback.hpp"
#include "database/objects/ArtistFeedbackBackendSync.hpp"
#include "database/objects/ReleaseFeedback.hpp"
#include "database/objects/ReleaseFeedbackBackendSync.hpp"
#include "database/objects/Track.hpp"

#include "Utils.hpp"

namespace lms::feedback::listenBrainz
{
    namespace detail
    {
        // ListenBrainz's feedback API only supports recordings: artist/release feedback never actually gets
        // delivered. Keep a PendingAdd sync placeholder in case this becomes supported in the future.
        template<typename FeedbackObjType, typename FeedbackObjBackendSyncType>
        void onFeedbackChanged(db::Session& session, typename FeedbackObjType::IdType id)
        {
            auto transaction{ session.createWriteTransaction() };

            if (auto feedbackObj{ FeedbackObjType::find(session, id) })
            {
                if (!FeedbackObjBackendSyncType::find(session, id, db::FeedbackBackend::ListenBrainz))
                    session.create<FeedbackObjBackendSyncType>(feedbackObj, db::FeedbackBackend::ListenBrainz);
            }
        }
    } // namespace detail

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

    void ListenBrainzBackend::requestImmediateImport(db::UserId userId)
    {
        _feedbacksSynchronizer.requestImmediateImport(userId);
    }

    void ListenBrainzBackend::requestImmediateExport(db::UserId userId)
    {
        _feedbacksSynchronizer.requestImmediateExport(userId);
    }

    void ListenBrainzBackend::onFeedbackChanged(db::ArtistFeedbackId id)
    {
        detail::onFeedbackChanged<db::ArtistFeedback, db::ArtistFeedbackBackendSync>(_db.getTLSSession(), id);
    }

    void ListenBrainzBackend::onFeedbackChanged(db::ReleaseFeedbackId id)
    {
        detail::onFeedbackChanged<db::ReleaseFeedback, db::ReleaseFeedbackBackendSync>(_db.getTLSSession(), id);
    }

    void ListenBrainzBackend::onFeedbackChanged(db::TrackFeedbackId id)
    {
        _feedbacksSynchronizer.enqueFeedback(id);
    }
} // namespace lms::feedback::listenBrainz
