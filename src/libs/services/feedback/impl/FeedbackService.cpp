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

#include "FeedbackService.hpp"
#include "FeedbackService.impl.hpp"

#include "services/database/Artist.hpp"
#include "services/database/Db.hpp"
#include "services/database/Release.hpp"
#include "services/database/Session.hpp"
#include "services/database/StarredArtist.hpp"
#include "services/database/StarredRelease.hpp"
#include "services/database/StarredTrack.hpp"
#include "services/database/Track.hpp"
#include "services/database/User.hpp"
#include "utils/ILogger.hpp"

#include "internal/InternalBackend.hpp"
#include "listenbrainz/ListenBrainzBackend.hpp"

namespace Feedback
{
    std::unique_ptr<IFeedbackService> createFeedbackService(boost::asio::io_context& ioContext, Db& db)
    {
        return std::make_unique<FeedbackService>(ioContext, db);
    }

    FeedbackService::FeedbackService(boost::asio::io_context& ioContext, Db& db)
        : _db{ db }
    {
        LMS_LOG(SCROBBLING, INFO, "Starting service...");
        _backends.emplace(Database::FeedbackBackend::Internal, std::make_unique<InternalBackend>(_db));
        _backends.emplace(Database::FeedbackBackend::ListenBrainz, std::make_unique<ListenBrainz::ListenBrainzBackend>(ioContext, _db));
        LMS_LOG(SCROBBLING, INFO, "Service started!");
    }

    FeedbackService::~FeedbackService()
    {
        LMS_LOG(SCROBBLING, INFO, "Service stopped!");
    }

    std::optional<Database::FeedbackBackend> FeedbackService::getUserFeedbackBackend(UserId userId)
    {
        std::optional<Database::FeedbackBackend> feedbackBackend;

        Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };
        if (const User::pointer user{ User::find(session, userId) })
            feedbackBackend = user->getFeedbackBackend();

        return feedbackBackend;
    }

    void FeedbackService::star(UserId userId, ArtistId artistId)
    {
        star<Artist, ArtistId, StarredArtist>(userId, artistId);
    }

    void FeedbackService::unstar(UserId userId, ArtistId artistId)
    {
        unstar<Artist, ArtistId, StarredArtist>(userId, artistId);
    }

    bool FeedbackService::isStarred(UserId userId, ArtistId artistId)
    {
        return isStarred<Artist, ArtistId, StarredArtist>(userId, artistId);
    }

    Wt::WDateTime FeedbackService::getStarredDateTime(UserId userId, ArtistId artistId)
    {
        return getStarredDateTime<Artist, ArtistId, StarredArtist>(userId, artistId);
    }

    FeedbackService::ArtistContainer FeedbackService::findStarredArtists(const ArtistFindParameters& params)
    {
        auto backend{ getUserFeedbackBackend(params.user) };
        if (!backend)
            return {};

        Artist::FindParameters searchParams;
        searchParams.setStarringUser(params.user, *backend);
        searchParams.setClusters(params.clusters);
        searchParams.setLinkType(params.linkType);
        searchParams.setSortMethod(params.sortMethod);
        searchParams.setRange(params.range);

        Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        return Artist::findIds(session, searchParams);
    }

    void FeedbackService::star(UserId userId, ReleaseId releaseId)
    {
        star<Release, ReleaseId, StarredRelease>(userId, releaseId);
    }

    void FeedbackService::unstar(UserId userId, ReleaseId releaseId)
    {
        unstar<Release, ReleaseId, StarredRelease>(userId, releaseId);
    }

    bool FeedbackService::isStarred(UserId userId, ReleaseId releaseId)
    {
        return isStarred<Release, ReleaseId, StarredRelease>(userId, releaseId);
    }

    Wt::WDateTime FeedbackService::getStarredDateTime(UserId userId, ReleaseId releaseId)
    {
        return getStarredDateTime<Release, ReleaseId, StarredRelease>(userId, releaseId);
    }

    FeedbackService::ReleaseContainer FeedbackService::findStarredReleases(const FindParameters& params)
    {
        auto backend{ getUserFeedbackBackend(params.user) };
        if (!backend)
            return {};

        Release::FindParameters searchParams;
        searchParams.setStarringUser(params.user, *backend);
        searchParams.setClusters(params.clusters);
        searchParams.setSortMethod(ReleaseSortMethod::StarredDateDesc);
        searchParams.setRange(params.range);

        Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        return Release::findIds(session, searchParams);
    }

    void FeedbackService::star(UserId userId, TrackId trackId)
    {
        star<Track, TrackId, StarredTrack>(userId, trackId);
    }

    void FeedbackService::unstar(UserId userId, TrackId trackId)
    {
        unstar<Track, TrackId, StarredTrack>(userId, trackId);
    }

    bool FeedbackService::isStarred(UserId userId, TrackId trackId)
    {
        return isStarred<Track, TrackId, StarredTrack>(userId, trackId);
    }

    Wt::WDateTime FeedbackService::getStarredDateTime(UserId userId, TrackId trackId)
    {
        return getStarredDateTime<Track, TrackId, StarredTrack>(userId, trackId);
    }

    FeedbackService::TrackContainer FeedbackService::findStarredTracks(const FindParameters& params)
    {
        auto backend{ getUserFeedbackBackend(params.user) };
        if (!backend)
            return {};

        Track::FindParameters searchParams;
        searchParams.setStarringUser(params.user, *backend);
        searchParams.setClusters(params.clusters);
        searchParams.setSortMethod(TrackSortMethod::StarredDateDesc);
        searchParams.setRange(params.range);

        Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        return Track::findIds(session, searchParams);
    }
} // ns Feedback

