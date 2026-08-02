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

#include "ListenBrainzBackend.hpp"

#include "core/IConfig.hpp"
#include "core/Service.hpp"
#include "core/http/IClient.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackArtistLink.hpp"

#include "Utils.hpp"

namespace lms::scrobbling::listenBrainz
{
    using namespace db;

    ListenBrainzBackend::ListenBrainzBackend(boost::asio::io_context& ioContext, db::IDb& db)
        : _ioContext{ ioContext }
        , _db{ db }
        , _baseAPIUrl{ core::Service<core::IConfig>::get()->getString("listenbrainz-api-base-url", "https://api.listenbrainz.org") }
        , _client{ core::http::createClient(_ioContext, _baseAPIUrl) }
        , _listensSynchronizer{ _ioContext, db, *_client }
    {
        LMS_LOG_LISTENBRAINZ(INFO, "Starting ListenBrainz backend... API endpoint = '" << _baseAPIUrl << "'");
    }

    ListenBrainzBackend::~ListenBrainzBackend()
    {
        LMS_LOG_LISTENBRAINZ(INFO, "Stopped ListenBrainz backend!");
    }

    void ListenBrainzBackend::listenStarted(const Listen& listen)
    {
        _listensSynchronizer.enqueListenNow(listen);
    }

    void ListenBrainzBackend::listenFinished(const TimedListen& listen, std::optional<std::chrono::seconds> /*duration*/)
    {
        _listensSynchronizer.enqueListen(listen);
    }

    void ListenBrainzBackend::addTimedListen(const TimedListen& listen)
    {
        _listensSynchronizer.enqueListen(listen);
    }

    bool ListenBrainzBackend::canBeScrobbled(TrackId trackId, std::optional<std::chrono::seconds> duration) const
    {
        Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        const Track::pointer track{ Track::find(session, trackId) };
        if (!track)
            return false;

        if (track->getArtistLinks(TrackArtistLinkType::Artist).empty())
        {
            LMS_LOG_LISTENBRAINZ(DEBUG, "Track cannot be scrobbled: no artist");
            return false;
        }

        if (duration)
        {
            const bool longEnough{ *duration >= std::chrono::minutes(4) || (*duration >= track->getDuration() / 2) };
            if (!longEnough)
            {
                LMS_LOG_LISTENBRAINZ(DEBUG, "Track cannot be scrobbled since played duration is too short: " << duration->count() << "s, total duration = " << std::chrono::duration_cast<std::chrono::seconds>(track->getDuration()).count() << "s");
                return false;
            }
        }

        return true;
    }

    void ListenBrainzBackend::requestImmediateImport(db::UserId userId)
    {
        _listensSynchronizer.requestImmediateImport(userId);
    }

    void ListenBrainzBackend::requestImmediateExport()
    {
        _listensSynchronizer.requestImmediateExport();
    }
} // namespace lms::scrobbling::listenBrainz
