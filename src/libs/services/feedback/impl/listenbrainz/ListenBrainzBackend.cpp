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
#include "database/objects/Track.hpp"

#include "Utils.hpp"

namespace lms::feedback::listenBrainz
{
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

    void ListenBrainzBackend::requestImmediateExport()
    {
        _feedbacksSynchronizer.requestImmediateExport();
    }

    bool ListenBrainzBackend::canBeFeedbacked(db::ArtistId /*artistId*/) const
    {
        // not supported by LB
        return false;
    }

    bool ListenBrainzBackend::canBeFeedbacked(db::ReleaseId /*releaseId*/) const
    {
        // not supported by LB
        return false;
    }

    bool ListenBrainzBackend::canBeFeedbacked(db::TrackId trackId) const
    {
        db::Session& session{ _db.getTLSSession() };
        return utils::canBeFeedbacked(session, trackId);
    }

    void ListenBrainzBackend::onFeedbackChanged(db::ArtistFeedbackId /*id*/)
    {
        // nothing to do
    }

    void ListenBrainzBackend::onFeedbackChanged(db::ReleaseFeedbackId /*id*/)
    {
        // nothing to do
    }

    void ListenBrainzBackend::onFeedbackChanged(db::TrackFeedbackId id)
    {
        _feedbacksSynchronizer.enqueFeedback(id);
    }
} // namespace lms::feedback::listenBrainz
