/*
 * Copyright (C) 2018 Emeric Poupon
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

#include "ClustersEngine.hpp"

#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Cluster.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackList.hpp"

namespace lms::recommendation
{
    using namespace db;

    std::unique_ptr<IEngine> createClustersEngine(db::IDb& db)
    {
        return std::make_unique<ClusterEngine>(db);
    }

    ClusterEngine::ClusterEngine(db::IDb& db)
        : _db{ db }
    {
    }

    ClusterEngine::~ClusterEngine() = default;

    void ClusterEngine::requestReload()
    {
        // nothing to do
    }

    bool ClusterEngine::isLoaded() const
    {
        return true;
    }

    TrackResults ClusterEngine::findSimilarTracks(std::span<const TrackId> trackIds, std::size_t maxCount) const
    {
        if (maxCount == 0)
            return {};

        Session& dbSession{ _db.getTLSSession() };
        auto transaction{ dbSession.createReadTransaction() };

        const std::vector<TrackId> trackIdsVector{ std::cbegin(trackIds), std::cend(trackIds) };
        auto similarTrackIds{ Track::findSimilarTrackIds(dbSession, trackIdsVector, Range{ 0, maxCount }) };
        TrackResults res;
        res.reserve(similarTrackIds.results.size());
        std::transform(std::cbegin(similarTrackIds.results), std::cend(similarTrackIds.results), std::back_inserter(res), [](const auto trackId) {
            return RecommendationResult<TrackId>{ .id = trackId, .distance = {} };
        });
        return res;
    }

    TrackResults ClusterEngine::findSimilarTracksFromTrackList(TrackListId tracklistId, std::size_t maxCount) const
    {
        TrackResults res;
        if (maxCount == 0)
            return res;

        {
            Session& dbSession{ _db.getTLSSession() };
            auto transaction{ dbSession.createReadTransaction() };

            const TrackList::pointer trackList{ TrackList::find(dbSession, tracklistId) };
            if (!trackList)
                return res;

            const auto tracks{ trackList->getSimilarTracks(0, maxCount) };
            res.reserve(tracks.size());
            std::transform(std::cbegin(tracks), std::cend(tracks), std::back_inserter(res), [](const auto& track) {
                return RecommendationResult<TrackId>{ .id = track->getId(), .distance = {} };
            });
        }

        return res;
    }

    ReleaseResults ClusterEngine::findSimilarReleases(ReleaseId releaseId, std::size_t maxCount) const
    {
        ReleaseResults res;
        if (maxCount == 0)
            return res;

        {
            Session& dbSession{ _db.getTLSSession() };
            auto transaction{ dbSession.createReadTransaction() };

            auto release{ Release::find(dbSession, releaseId) };
            if (!release)
                return res;

            const auto releases{ release->findSimilarReleases(0, maxCount) };
            res.reserve(releases.size());
            std::transform(std::cbegin(releases), std::cend(releases), std::back_inserter(res), [](const auto& release) {
                return RecommendationResult<ReleaseId>{ .id = release->getId(), .distance = {} };
            });
        }

        return res;
    }

    ArtistResults ClusterEngine::findSimilarArtists(ArtistId artistId, core::EnumSet<TrackArtistLinkType> linkTypes, std::size_t maxCount) const
    {
        if (maxCount == 0)
            return {};

        Session& dbSession{ _db.getTLSSession() };
        auto transaction{ dbSession.createReadTransaction() };

        auto artist{ Artist::find(dbSession, artistId) };
        if (!artist)
            return {};

        auto similarArtistIds{ artist->findSimilarArtistIds(linkTypes, Range{ 0, maxCount }) };
        ArtistResults res;
        res.reserve(similarArtistIds.results.size());
        std::transform(std::cbegin(similarArtistIds.results), std::cend(similarArtistIds.results), std::back_inserter(res), [](const auto id) {
            return RecommendationResult<ArtistId>{ .id = id, .distance = {} };
        });
        return res;
    }

    TrackResults ClusterEngine::findTrackSimilarityPath(db::TrackId startTrackId, db::TrackId endTrackId, std::size_t maxCount) const
    {
        if (maxCount == 0)
            return {};

        if (startTrackId == endTrackId)
            return { RecommendationResult<TrackId>{ .id = startTrackId, .distance = {} } };

        Session& dbSession{ _db.getTLSSession() };
        auto transaction{ dbSession.createReadTransaction() };

        const auto startTrack{ Track::find(dbSession, startTrackId) };
        const auto endTrack{ Track::find(dbSession, endTrackId) };
        if (!startTrack || !endTrack)
            return {};

        TrackResults res;
        res.reserve(std::min<std::size_t>(maxCount, 2));
        res.push_back({ .id = startTrackId, .distance = {} });
        if (maxCount > 1)
            res.push_back({ .id = endTrackId, .distance = {} });

        return res;
    }

} // namespace lms::recommendation
