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

#include "TagsEngine.hpp"

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <unordered_set>

#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Genre.hpp"
#include "database/objects/Grouping.hpp"
#include "database/objects/Language.hpp"
#include "database/objects/Mood.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackList.hpp"

#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"

#include "track-selection-constraints/DuplicateTrackConstraint.hpp"
#include "track-selection-constraints/SameArtistConstraint.hpp"
#include "track-selection-constraints/SameRecordingMBIDConstraint.hpp"
#include "track-selection-constraints/SameReleaseConstraint.hpp"
#include "track-selection-constraints/TrackCandidateContext.hpp"

#define LOG(sev, message) LMS_LOG(RECOMMENDATION, sev, "[tags] " << message)

namespace lms::recommendation
{
    namespace
    {
        template<typename IdType>
        std::vector<std::pair<IdType, std::size_t>> computeTagOverlap(
            const std::unordered_map<IdType, std::vector<TagId>>& profileMap,
            const std::unordered_set<IdType>& excludeIds,
            const std::unordered_set<TagId>& queryTags)
        {
            std::vector<std::pair<IdType, std::size_t>> results;

            for (const auto& [candidateId, candidateTags] : profileMap)
            {
                if (excludeIds.contains(candidateId))
                    continue;

                std::size_t count{};
                for (const TagId& tagId : candidateTags)
                {
                    if (queryTags.contains(tagId))
                        ++count;
                }

                if (count > 0)
                    results.emplace_back(candidateId, count);
            }

            return results;
        }

        template<typename IdType>
        ResultContainer<IdType> findSimilarByTagOverlap(
            const std::unordered_map<IdType, std::vector<TagId>>& profileMap,
            IdType queryId,
            const std::vector<TagId>& queryTags,
            std::size_t maxCount)
        {
            const std::unordered_set<TagId> querySet{ queryTags.cbegin(), queryTags.cend() };
            auto overlapCounts{ computeTagOverlap(profileMap, { queryId }, querySet) };

            const std::size_t resultCount{ std::min(maxCount, overlapCounts.size()) };
            std::partial_sort(overlapCounts.begin(), std::next(overlapCounts.begin(), resultCount), overlapCounts.end(),
                              [](const auto& a, const auto& b) { return a.second > b.second; });

            ResultContainer<IdType> res;
            res.reserve(resultCount);
            for (std::size_t i{}; i < resultCount; ++i)
                res.push_back({ .id = overlapCounts[i].first, .distanceToFirst = {}, .distanceToPrevious = {} });

            return res;
        }
    } // namespace

    std::unique_ptr<IEngine> createTagsEngine(db::IDb& db)
    {
        return std::make_unique<TagsEngine>(db);
    }

    TagsEngine::TagsEngine(db::IDb& db)
        : _db{ db }
    {
        constexpr float sameReleaseWeight{ 0.5F };
        constexpr float sameArtistWeight{ 0.5F };

        _trackEvaluator.addHardConstraint(std::make_unique<DuplicateTrackConstraint>());
        _trackEvaluator.addHardConstraint(std::make_unique<SameRecordingMBIDConstraint>(_trackMetadata));
        _trackEvaluator.addSoftConstraint(std::make_unique<SameReleaseConstraint>(_trackMetadata), sameReleaseWeight);
        _trackEvaluator.addSoftConstraint(std::make_unique<SameArtistConstraint>(_trackMetadata), sameArtistWeight);
    }

    TagsEngine::~TagsEngine() = default;

    void TagsEngine::load()
    {
        LMS_SCOPED_TRACE_OVERVIEW("TagsEngine", "Loading");
        LOG(INFO, "loading...");

        _trackMetadata.clear();
        _trackTags.clear();
        _releaseTags.clear();
        _artistTags.clear();

        db::Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        buildTrackTags(session);
        buildTrackMetadata(session);
        buildReleaseTags();
        buildArtistTags();

        LOG(INFO, "loaded " << _trackTags.size() << " tracks, " << _releaseTags.size() << " releases, " << _artistTags.size() << " artists");
    }

    void TagsEngine::buildTrackMetadata(db::Session& session)
    {
        LOG(DEBUG, "building track metadata...");

        for (const auto& [trackId, tags] : _trackTags)
            _trackMetadata.try_emplace(trackId);

        db::Track::find(session, db::Track::FindParameters{}, [&](const db::Track::pointer& track) {
            const auto it{ _trackMetadata.find(track->getId()) };
            if (it != _trackMetadata.cend())
            {
                it->second.releaseId = track->getReleaseId();
                it->second.recordingMBID = track->getRecordingMBID();
            }
        });

        db::Artist::find(session, db::Artist::FindParameters{}, [&](const db::Artist::pointer& artist) {
            const auto mbid{ artist->getMBID() };
            // skip "Various Artists" to avoid false artist matches
            if (mbid && mbid->toString() == "89ad4ac3-39f7-470e-963a-56509c546377")
                return;

            std::unordered_set<db::TrackId> artistTrackIds;

            {
                db::Release::FindParameters params;
                params.setArtist(artist->getId());

                for (const db::ReleaseId releaseId : db::Release::findIds(session, params))
                {
                    db::Track::FindParameters trackParams;
                    trackParams.setRelease(releaseId);
                    for (const db::TrackId trackId : db::Track::findIds(session, trackParams))
                        artistTrackIds.insert(trackId);
                }
            }

            for (const db::TrackId trackId : artistTrackIds)
                _trackMetadata[trackId].artistIds.push_back(artist->getId());
        });

        for (auto& [trackId, metadata] : _trackMetadata)
            std::sort(metadata.artistIds.begin(), metadata.artistIds.end());
    }

    void TagsEngine::buildTrackTags(db::Session& session)
    {
        LOG(DEBUG, "building track tags...");

        db::Genre::find(session, db::Genre::FindParameters{}, [&](const db::Genre::pointer& genre) {
            const TagId tagId{ .type = TagId::Type::Genre, .id = genre->getId() };

            db::Track::FindParameters params;
            params.filters.setGenre(genre->getId());

            db::Track::find(session, params, [&](const db::Track::pointer& track) {
                _trackTags[track->getId()].push_back(tagId);
            });
        });

        db::Mood::find(session, db::Mood::FindParameters{}, [&](const db::Mood::pointer& mood) {
            const TagId tagId{ .type = TagId::Type::Mood, .id = mood->getId() };

            db::Track::FindParameters params;
            params.filters.setMood(mood->getId());

            db::Track::find(session, params, [&](const db::Track::pointer& track) {
                _trackTags[track->getId()].push_back(tagId);
            });
        });

        db::Grouping::find(session, db::Grouping::FindParameters{}, [&](const db::Grouping::pointer& grouping) {
            const TagId tagId{ .type = TagId::Type::Grouping, .id = grouping->getId() };

            db::Track::FindParameters params;
            params.filters.setGrouping(grouping->getId());

            db::Track::find(session, params, [&](const db::Track::pointer& track) {
                _trackTags[track->getId()].push_back(tagId);
            });
        });

        db::Language::find(session, db::Language::FindParameters{}, [&](const db::Language::pointer& language) {
            const TagId tagId{ .type = TagId::Type::Language, .id = language->getId() };

            db::Track::FindParameters params;
            params.filters.setLanguage(language->getId());

            db::Track::find(session, params, [&](const db::Track::pointer& track) {
                _trackTags[track->getId()].push_back(tagId);
            });
        });
    }

    void TagsEngine::buildReleaseTags()
    {
        LOG(DEBUG, "building release tags...");

        for (const auto& [trackId, tags] : _trackTags)
        {
            const auto metaIt{ _trackMetadata.find(trackId) };
            if (metaIt == _trackMetadata.cend())
                continue;

            if (const db::ReleaseId releaseId{ metaIt->second.releaseId }; releaseId.isValid())
            {
                for (const TagId& tagId : tags)
                    _releaseTags[releaseId].push_back(tagId);
            }
        }

        for (auto& [releaseId, tags] : _releaseTags)
        {
            std::sort(tags.begin(), tags.end());
            tags.erase(std::unique(tags.begin(), tags.end()), tags.end());
        }
    }

    void TagsEngine::buildArtistTags()
    {
        LOG(DEBUG, "building artist tags...");

        for (const auto& [trackId, tags] : _trackTags)
        {
            const auto metaIt{ _trackMetadata.find(trackId) };
            if (metaIt == _trackMetadata.cend())
                continue;

            for (const db::ArtistId artistId : metaIt->second.artistIds)
            {
                for (const TagId& tagId : tags)
                    _artistTags[artistId].push_back(tagId);
            }
        }

        for (auto& [artistId, tags] : _artistTags)
        {
            std::sort(tags.begin(), tags.end());
            tags.erase(std::unique(tags.begin(), tags.end()), tags.end());
        }
    }

    TrackResults TagsEngine::findSimilarTracks(std::span<const db::TrackId> trackIds, std::size_t maxCount) const
    {
        LMS_SCOPED_TRACE_DETAILED("TagsEngine", "Find similar tracks");

        if (maxCount == 0 || trackIds.empty())
            return {};

        std::unordered_set<TagId> queryTags;
        for (const db::TrackId trackId : trackIds)
        {
            const auto it{ _trackTags.find(trackId) };
            if (it != _trackTags.cend())
            {
                for (const TagId& tagId : it->second)
                    queryTags.insert(tagId);
            }
        }

        if (queryTags.empty())
            return {};

        const std::unordered_set<db::TrackId> excludeSet{ std::cbegin(trackIds), std::cend(trackIds) };
        auto overlapCounts{ computeTagOverlap(_trackTags, excludeSet, queryTags) };

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

    TrackResults TagsEngine::greedySelect(std::vector<db::TrackId> candidates, std::vector<db::TrackId> selectedTracks, std::size_t maxCount) const
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
                    .seedTrackIds = {},
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

            res.push_back({ .id = candidates[*bestIdx], .distanceToFirst = {}, .distanceToPrevious = {} });
            selectedTracks.push_back(candidates[*bestIdx]);
            candidates.erase(std::begin(candidates) + static_cast<std::ptrdiff_t>(*bestIdx));
        }

        return res;
    }

    TrackResults TagsEngine::findSimilarTracksFromTrackList(db::TrackListId tracklistId, std::size_t maxCount) const
    {
        LMS_SCOPED_TRACE_DETAILED("TagsEngine", "Find similar tracks from tracklist");

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

    ReleaseResults TagsEngine::findSimilarReleases(db::ReleaseId releaseId, std::size_t maxCount) const
    {
        LMS_SCOPED_TRACE_DETAILED("TagsEngine", "Find similar releases");

        if (maxCount == 0)
            return {};

        const auto queryIt{ _releaseTags.find(releaseId) };
        if (queryIt == _releaseTags.cend() || queryIt->second.empty())
            return {};

        return findSimilarByTagOverlap<db::ReleaseId>(_releaseTags, releaseId, queryIt->second, maxCount);
    }

    ArtistResults TagsEngine::findSimilarArtists(db::ArtistId artistId, core::EnumSet<db::TrackArtistLinkType> linkTypes, std::size_t maxCount) const
    {
        LMS_SCOPED_TRACE_DETAILED("TagsEngine", "Find similar artists");

        if (maxCount == 0 || !linkTypes.contains(db::TrackArtistLinkType::Artist))
            return {};

        const auto queryIt{ _artistTags.find(artistId) };
        if (queryIt == _artistTags.cend() || queryIt->second.empty())
            return {};

        return findSimilarByTagOverlap<db::ArtistId>(_artistTags, artistId, queryIt->second, maxCount);
    }

    TrackResults TagsEngine::findTrackSimilarityPath(db::TrackId startTrackId, db::TrackId endTrackId, std::size_t maxCount) const
    {
        LMS_SCOPED_TRACE_DETAILED("TagsEngine", "Find track similarity path");

        if (maxCount == 0)
            return {};

        if (startTrackId == endTrackId)
            return { RecommendationResult<db::TrackId>{ .id = startTrackId, .distanceToFirst = {}, .distanceToPrevious = {} } };

        db::Session& dbSession{ _db.getTLSSession() };
        auto transaction{ dbSession.createReadTransaction() };

        const auto startTrack{ db::Track::find(dbSession, startTrackId) };
        const auto endTrack{ db::Track::find(dbSession, endTrackId) };
        if (!startTrack || !endTrack)
            return {};

        TrackResults res;
        res.reserve(std::min<std::size_t>(maxCount, 2));
        res.push_back({ .id = startTrackId, .distanceToFirst = {}, .distanceToPrevious = {} });
        if (maxCount > 1)
            res.push_back({ .id = endTrackId, .distanceToFirst = {}, .distanceToPrevious = {} });

        return res;
    }

} // namespace lms::recommendation

#undef LOG
