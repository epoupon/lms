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

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <unordered_set>

#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Cluster.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackList.hpp"

#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"

#include "track-selection-constraints/DuplicateTrackConstraint.hpp"
#include "track-selection-constraints/SameArtistConstraint.hpp"
#include "track-selection-constraints/SameReleaseConstraint.hpp"
#include "track-selection-constraints/TrackCandidateContext.hpp"

#define LOG(sev, message) LMS_LOG(RECOMMENDATION, sev, "[clusters] " << message)

namespace lms::recommendation
{
    namespace
    {
        template<typename IdType>
        std::vector<std::pair<IdType, std::size_t>> computeClusterOverlap(
            const std::unordered_map<IdType, std::vector<db::ClusterId>>& profileMap,
            const std::unordered_set<IdType>& excludeIds,
            const std::unordered_set<db::ClusterId>& queryClusters)
        {
            std::vector<std::pair<IdType, std::size_t>> results;
            for (const auto& [candidateId, candidateClusters] : profileMap)
            {
                if (excludeIds.contains(candidateId))
                    continue;
                std::size_t count{};
                for (const db::ClusterId clusterId : candidateClusters)
                    if (queryClusters.contains(clusterId))
                        ++count;
                if (count > 0)
                    results.emplace_back(candidateId, count);
            }
            return results;
        }

        template<typename IdType>
        ResultContainer<IdType> findSimilarByClusterOverlap(
            const std::unordered_map<IdType, std::vector<db::ClusterId>>& profileMap,
            IdType queryId,
            const std::vector<db::ClusterId>& queryClusters,
            std::size_t maxCount)
        {
            const std::unordered_set<db::ClusterId> querySet{ queryClusters.cbegin(), queryClusters.cend() };
            auto overlapCounts{ computeClusterOverlap(profileMap, { queryId }, querySet) };

            const std::size_t resultCount{ std::min(maxCount, overlapCounts.size()) };
            std::partial_sort(overlapCounts.begin(), std::next(overlapCounts.begin(), resultCount), overlapCounts.end(),
                              [](const auto& a, const auto& b) { return a.second > b.second; });

            ResultContainer<IdType> res;
            res.reserve(resultCount);
            for (std::size_t i{}; i < resultCount; ++i)
                res.push_back({ .id = overlapCounts[i].first, .distance = {} });

            return res;
        }
    } // namespace

    std::unique_ptr<IEngine> createClustersEngine(db::IDb& db)
    {
        return std::make_unique<ClusterEngine>(db);
    }

    ClusterEngine::ClusterEngine(db::IDb& db)
        : _db{ db }
    {
        constexpr float sameReleaseWeight{ 0.5F };
        constexpr float sameArtistWeight{ 0.5F };
        _trackEvaluator.addHardConstraint(std::make_unique<DuplicateTrackConstraint>());
        _trackEvaluator.addSoftConstraint(std::make_unique<SameReleaseConstraint>(_trackMetadata), sameReleaseWeight);
        _trackEvaluator.addSoftConstraint(std::make_unique<SameArtistConstraint>(_trackMetadata), sameArtistWeight);
    }

    ClusterEngine::~ClusterEngine() = default;

    void ClusterEngine::load()
    {
        LMS_SCOPED_TRACE_OVERVIEW("ClustersEngine", "Loading");
        LOG(INFO, "loading...");

        _trackMetadata.clear();
        _trackClusters.clear();
        _releaseClusters.clear();
        _artistClusters.clear();

        db::Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        buildTrackMetadata(session);
        buildTrackClusters(session);
        buildReleaseClusters();
        buildArtistClusters();

        LOG(INFO, "loaded " << _trackClusters.size() << " tracks, " << _releaseClusters.size() << " releases, " << _artistClusters.size() << " artists");
    }

    void ClusterEngine::buildTrackMetadata(db::Session& session)
    {
        LOG(DEBUG, "building track metadata...");

        db::Release::find(session, db::Release::FindParameters{}, [&](const db::Release::pointer& release) {
            db::Track::FindParameters params;
            params.setRelease(release->getId());
            for (const db::TrackId trackId : db::Track::findIds(session, params).results)
                _trackMetadata[trackId].releaseId = release->getId();
        });

        db::Artist::find(session, db::Artist::FindParameters{}, [&](const db::Artist::pointer& artist) {
            std::unordered_set<db::TrackId> artistTrackIds;

            {
                db::Track::FindParameters params;
                params.setArtist(artist->getId(), { db::TrackArtistLinkType::Artist });
                for (const db::TrackId trackId : db::Track::findIds(session, params).results)
                    artistTrackIds.insert(trackId);
            }

            {
                db::Release::FindParameters params;
                params.setArtist(artist->getId());
                for (const db::ReleaseId releaseId : db::Release::findIds(session, params).results)
                {
                    db::Track::FindParameters trackParams;
                    trackParams.setRelease(releaseId);
                    for (const db::TrackId trackId : db::Track::findIds(session, trackParams).results)
                        artistTrackIds.insert(trackId);
                }
            }

            for (const db::TrackId trackId : artistTrackIds)
                _trackMetadata[trackId].artistIds.push_back(artist->getId());
        });

        for (auto& [trackId, metadata] : _trackMetadata)
            std::sort(metadata.artistIds.begin(), metadata.artistIds.end());
    }

    void ClusterEngine::buildTrackClusters(db::Session& session)
    {
        LOG(DEBUG, "building track clusters...");

        db::Cluster::find(session, db::Cluster::FindParameters{}, [&](const db::Cluster::pointer& cluster) {
            const db::ClusterId clusterId{ cluster->getId() };
            for (const db::TrackId trackId : cluster->getTracks().results)
                _trackClusters[trackId].push_back(clusterId);
        });
    }

    void ClusterEngine::buildReleaseClusters()
    {
        LOG(DEBUG, "building release clusters...");

        for (const auto& [trackId, clusters] : _trackClusters)
        {
            const auto metaIt{ _trackMetadata.find(trackId) };
            if (metaIt == _trackMetadata.cend())
                continue;

            if (const db::ReleaseId releaseId{ metaIt->second.releaseId }; releaseId.isValid())
                for (const db::ClusterId clusterId : clusters)
                    _releaseClusters[releaseId].push_back(clusterId);
        }

        for (auto& [_, clusters] : _releaseClusters)
        {
            std::sort(clusters.begin(), clusters.end());
            clusters.erase(std::unique(clusters.begin(), clusters.end()), clusters.end());
        }
    }

    void ClusterEngine::buildArtistClusters()
    {
        LOG(DEBUG, "building artist clusters...");

        for (const auto& [trackId, clusters] : _trackClusters)
        {
            const auto metaIt{ _trackMetadata.find(trackId) };
            if (metaIt == _trackMetadata.cend())
                continue;

            for (const db::ArtistId artistId : metaIt->second.artistIds)
                for (const db::ClusterId clusterId : clusters)
                    _artistClusters[artistId].push_back(clusterId);
        }

        for (auto& [_, clusters] : _artistClusters)
        {
            std::sort(clusters.begin(), clusters.end());
            clusters.erase(std::unique(clusters.begin(), clusters.end()), clusters.end());
        }
    }

    TrackResults ClusterEngine::findSimilarTracks(std::span<const db::TrackId> trackIds, std::size_t maxCount) const
    {
        LMS_SCOPED_TRACE_DETAILED("ClustersEngine", "Find similar tracks");

        if (maxCount == 0 || trackIds.empty())
            return {};

        std::unordered_set<db::ClusterId> queryClusters;
        for (const db::TrackId trackId : trackIds)
        {
            const auto it{ _trackClusters.find(trackId) };
            if (it != _trackClusters.cend())
                for (const db::ClusterId clusterId : it->second)
                    queryClusters.insert(clusterId);
        }

        if (queryClusters.empty())
            return {};

        const std::unordered_set<db::TrackId> excludeSet{ std::cbegin(trackIds), std::cend(trackIds) };
        auto overlapCounts{ computeClusterOverlap(_trackClusters, excludeSet, queryClusters) };

        static constexpr std::size_t oversamplingFactor{ 5 };
        const std::size_t candidateCount{ std::min(maxCount * oversamplingFactor, overlapCounts.size()) };
        std::partial_sort(overlapCounts.begin(), std::next(overlapCounts.begin(), candidateCount), overlapCounts.end(),
                          [](const auto& a, const auto& b) { return a.second > b.second; });
        overlapCounts.resize(candidateCount);

        std::vector<db::TrackId> candidates;
        candidates.reserve(candidateCount);
        for (const auto& [trackId, count] : overlapCounts)
            candidates.push_back(trackId);

        std::vector<db::TrackId> seeds{ std::cbegin(trackIds), std::cend(trackIds) };
        return greedySelect(std::move(candidates), std::move(seeds), maxCount);
    }

    TrackResults ClusterEngine::greedySelect(std::vector<db::TrackId> candidates, std::vector<db::TrackId> selectedTracks, std::size_t maxCount) const
    {
        selectedTracks.reserve(selectedTracks.size() + maxCount);

        TrackResults res;
        res.reserve(maxCount);

        while (res.size() < maxCount && !candidates.empty())
        {
            std::optional<std::size_t> bestIdx;
            float bestScore{ std::numeric_limits<float>::max() };

            for (std::size_t i{}; i < candidates.size(); ++i)
            {
                const TrackCandidateContext context{
                    .candidateTrackId = candidates[i],
                    .selectedTracks = selectedTracks,
                };

                if (_trackEvaluator.rejects(context))
                    continue;

                const float score{ _trackEvaluator.score(context) };
                if (score < bestScore)
                {
                    bestScore = score;
                    bestIdx = i;
                }
            }

            if (!bestIdx)
                break;

            res.push_back({ .id = candidates[*bestIdx], .distance = {} });
            selectedTracks.push_back(candidates[*bestIdx]);
            candidates.erase(std::begin(candidates) + static_cast<std::ptrdiff_t>(*bestIdx));
        }

        return res;
    }

    TrackResults ClusterEngine::findSimilarTracksFromTrackList(db::TrackListId tracklistId, std::size_t maxCount) const
    {
        LMS_SCOPED_TRACE_DETAILED("ClustersEngine", "Find similar tracks from tracklist");

        if (maxCount == 0)
            return {};

        std::vector<db::TrackId> trackIds;
        {
            db::Session& dbSession{ _db.getTLSSession() };
            auto transaction{ dbSession.createReadTransaction() };

            const db::TrackList::pointer trackList{ db::TrackList::find(dbSession, tracklistId) };
            if (!trackList)
                return {};

            trackIds = trackList->getTrackIds();
        }

        if (trackIds.empty())
            return {};

        return findSimilarTracks(trackIds, maxCount);
    }

    ReleaseResults ClusterEngine::findSimilarReleases(db::ReleaseId releaseId, std::size_t maxCount) const
    {
        LMS_SCOPED_TRACE_DETAILED("ClustersEngine", "Find similar releases");

        if (maxCount == 0)
            return {};

        const auto queryIt{ _releaseClusters.find(releaseId) };
        if (queryIt == _releaseClusters.cend() || queryIt->second.empty())
            return {};

        return findSimilarByClusterOverlap<db::ReleaseId>(_releaseClusters, releaseId, queryIt->second, maxCount);
    }

    ArtistResults ClusterEngine::findSimilarArtists(db::ArtistId artistId, core::EnumSet<db::TrackArtistLinkType> linkTypes, std::size_t maxCount) const
    {
        LMS_SCOPED_TRACE_DETAILED("ClustersEngine", "Find similar artists");

        if (maxCount == 0 || !linkTypes.contains(db::TrackArtistLinkType::Artist))
            return {};

        const auto queryIt{ _artistClusters.find(artistId) };
        if (queryIt == _artistClusters.cend() || queryIt->second.empty())
            return {};

        return findSimilarByClusterOverlap<db::ArtistId>(_artistClusters, artistId, queryIt->second, maxCount);
    }

    TrackResults ClusterEngine::findTrackSimilarityPath(db::TrackId startTrackId, db::TrackId endTrackId, std::size_t maxCount) const
    {
        LMS_SCOPED_TRACE_DETAILED("ClustersEngine", "Find track similarity path");

        if (maxCount == 0)
            return {};

        if (startTrackId == endTrackId)
            return { RecommendationResult<db::TrackId>{ .id = startTrackId, .distance = {} } };

        db::Session& dbSession{ _db.getTLSSession() };
        auto transaction{ dbSession.createReadTransaction() };

        const auto startTrack{ db::Track::find(dbSession, startTrackId) };
        const auto endTrack{ db::Track::find(dbSession, endTrackId) };
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

#undef LOG
