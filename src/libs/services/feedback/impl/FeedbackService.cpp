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

#include "FeedbackService.impl.hpp"

#include "core/ILogger.hpp"
#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/ArtistFeedback.hpp"
#include "database/objects/RatedArtist.hpp"
#include "database/objects/RatedRelease.hpp"
#include "database/objects/RatedTrack.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/ReleaseFeedback.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackFeedback.hpp"
#include "database/objects/TrackFeedbackBackendSync.hpp"
#include "database/objects/User.hpp"

#include "listenbrainz/ListenBrainzBackend.hpp"

namespace lms::feedback
{
    std::unique_ptr<IFeedbackService> createFeedbackService(boost::asio::io_context& ioContext, db::IDb& db)
    {
        return std::make_unique<FeedbackService>(ioContext, db);
    }

    FeedbackService::FeedbackService(boost::asio::io_context& ioContext, db::IDb& db)
        : _db{ db }
    {
        LMS_LOG(SCROBBLING, INFO, "Starting service...");

        _backends.emplace(db::FeedbackBackend::ListenBrainz, std::make_unique<listenBrainz::ListenBrainzBackend>(ioContext, _db));

        LMS_LOG(SCROBBLING, INFO, "Service started!");
    }

    FeedbackService::~FeedbackService()
    {
        LMS_LOG(SCROBBLING, INFO, "Service stopped!");
    }

    core::EnumSet<db::FeedbackBackend> FeedbackService::getUserFeedbackBackends(db::UserId userId)
    {
        core::EnumSet<db::FeedbackBackend> backends;

        db::Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };
        if (const db::User::pointer user{ db::User::find(session, userId) })
            backends = user->getFeedbackBackends();

        return backends;
    }

    void FeedbackService::requestImmediateImport(db::UserId userId, db::FeedbackBackend backend)
    {
        if (!getUserFeedbackBackends(userId).contains(backend))
            return;

        _backends[backend]->requestImmediateImport(userId);
    }

    void FeedbackService::requestImmediateExport(db::UserId userId, db::FeedbackBackend backend)
    {
        if (!getUserFeedbackBackends(userId).contains(backend))
            return;

        markPendingExports(userId, backend);
        _backends[backend]->requestImmediateExport();
    }

    void FeedbackService::markPendingExports(db::UserId userId, db::FeedbackBackend backend)
    {
        IFeedbackBackend& backendImpl{ *_backends[backend] };

        constexpr std::size_t chunkSize{ 500 };
        db::TrackFeedbackId lastRetrievedId;
        for (;;)
        {
            std::vector<db::TrackFeedbackId> ids;
            std::size_t rawCount{};
            {
                db::Session& session{ _db.getTLSSession() };
                auto transaction{ session.createReadTransaction() };

                db::TrackFeedback::FindParameters params;
                params.setUser(userId);
                params.setLastRetrievedId(lastRetrievedId);
                params.setRange(db::Range{ 0, chunkSize });
                params.setSortMethod(db::TrackFeedbackSortMethod::Id);
                db::TrackFeedback::find(session, params, [&](const db::TrackFeedback::pointer& feedback) {
                    ++rawCount;
                    lastRetrievedId = feedback->getId();
                    if (feedback->getValue() != db::FeedbackValue::None)
                        ids.push_back(feedback->getId());
                });
            }

            if (rawCount == 0)
                break;

            {
                db::Session& session{ _db.getTLSSession() };
                auto transaction{ session.createWriteTransaction() };

                for (const db::TrackFeedbackId id : ids)
                {
                    if (db::TrackFeedbackBackendSync::find(session, id, backend))
                        continue;

                    const db::TrackFeedback::pointer trackFeedback{ db::TrackFeedback::find(session, id) };
                    if (!trackFeedback || !backendImpl.canBeFeedbacked(trackFeedback->getTrack()->getId()))
                        continue;

                    session.create<db::TrackFeedbackBackendSync>(trackFeedback, backend);
                }
            }

            if (rawCount < chunkSize)
                break;
        }
    }

    void FeedbackService::setFeedback(db::UserId userId, db::ArtistId artistId, db::FeedbackValue value)
    {
        setFeedback<db::Artist, db::ArtistId, db::ArtistFeedback>(userId, artistId, value);
    }

    db::FeedbackValue FeedbackService::getFeedback(db::UserId userId, db::ArtistId artistId)
    {
        return getFeedback<db::Artist, db::ArtistId, db::ArtistFeedback>(userId, artistId);
    }

    Wt::WDateTime FeedbackService::getFeedbackDateTime(db::UserId userId, db::ArtistId artistId)
    {
        return getFeedbackDateTime<db::Artist, db::ArtistId, db::ArtistFeedback>(userId, artistId);
    }

    FeedbackService::ArtistContainer FeedbackService::findArtistsByFeedback(const ArtistFindParameters& params)
    {
        db::Artist::FindParameters searchParams;
        searchParams.setFilters(params.filters);
        searchParams.setFeedbackUser(params.user);
        searchParams.setFeedbackValue(params.feedbackValue);
        searchParams.setKeywords(params.keywords);
        searchParams.setTrackArtistLinkType(params.trackArtistLinkType);
        searchParams.setSortMethod(params.sortMethod);
        searchParams.setRange(params.range);

        db::Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        return db::Artist::findIds(session, searchParams);
    }

    void FeedbackService::setRating(db::UserId userId, db::ArtistId artistId, std::optional<db::Rating> rating)
    {
        setRating<db::Artist, db::ArtistId, db::RatedArtist>(userId, artistId, rating);
    }

    std::optional<db::Rating> FeedbackService::getRating(db::UserId userId, db::ArtistId artistId)
    {
        return getRating<db::Artist, db::ArtistId, db::RatedArtist>(userId, artistId);
    }

    void FeedbackService::setFeedback(db::UserId userId, db::ReleaseId releaseId, db::FeedbackValue value)
    {
        setFeedback<db::Release, db::ReleaseId, db::ReleaseFeedback>(userId, releaseId, value);
    }

    db::FeedbackValue FeedbackService::getFeedback(db::UserId userId, db::ReleaseId releaseId)
    {
        return getFeedback<db::Release, db::ReleaseId, db::ReleaseFeedback>(userId, releaseId);
    }

    Wt::WDateTime FeedbackService::getFeedbackDateTime(db::UserId userId, db::ReleaseId releaseId)
    {
        return getFeedbackDateTime<db::Release, db::ReleaseId, db::ReleaseFeedback>(userId, releaseId);
    }

    FeedbackService::ReleaseContainer FeedbackService::findReleasesByFeedback(const FindParameters& params)
    {
        db::Release::FindParameters searchParams;
        searchParams.setFeedbackUser(params.user);
        searchParams.setFeedbackValue(params.feedbackValue);
        searchParams.setFilters(params.filters);
        searchParams.setKeywords(params.keywords);
        searchParams.setSortMethod(db::ReleaseSortMethod::FeedbackDateDesc);
        searchParams.setRange(params.range);

        db::Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        return db::Release::findIds(session, searchParams);
    }

    void FeedbackService::setRating(db::UserId userId, db::ReleaseId releaseId, std::optional<db::Rating> rating)
    {
        setRating<db::Release, db::ReleaseId, db::RatedRelease>(userId, releaseId, rating);
    }

    std::optional<db::Rating> FeedbackService::getRating(db::UserId userId, db::ReleaseId releaseId)
    {
        return getRating<db::Release, db::ReleaseId, db::RatedRelease>(userId, releaseId);
    }

    void FeedbackService::setFeedback(db::UserId userId, db::TrackId trackId, db::FeedbackValue value)
    {
        setFeedback<db::Track, db::TrackId, db::TrackFeedback>(userId, trackId, value);
    }

    db::FeedbackValue FeedbackService::getFeedback(db::UserId userId, db::TrackId trackId)
    {
        return getFeedback<db::Track, db::TrackId, db::TrackFeedback>(userId, trackId);
    }

    Wt::WDateTime FeedbackService::getFeedbackDateTime(db::UserId userId, db::TrackId trackId)
    {
        return getFeedbackDateTime<db::Track, db::TrackId, db::TrackFeedback>(userId, trackId);
    }

    FeedbackService::TrackContainer FeedbackService::findTracksByFeedback(const FindParameters& params)
    {
        db::Track::FindParameters searchParams;
        searchParams.setFeedbackUser(params.user);
        searchParams.setFeedbackValue(params.feedbackValue);
        searchParams.setFilters(params.filters);
        searchParams.setKeywords(params.keywords);
        searchParams.setSortMethod(db::TrackSortMethod::FeedbackDateDesc);
        searchParams.setRange(params.range);

        db::Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        return db::Track::findIds(session, searchParams);
    }

    void FeedbackService::setRating(db::UserId userId, db::TrackId trackId, std::optional<db::Rating> rating)
    {
        setRating<db::Track, db::TrackId, db::RatedTrack>(userId, trackId, rating);
    }

    std::optional<db::Rating> FeedbackService::getRating(db::UserId userId, db::TrackId trackId)
    {
        return getRating<db::Track, db::TrackId, db::RatedTrack>(userId, trackId);
    }
} // namespace lms::feedback
