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

#pragma once

#include <span>
#include <unordered_map>
#include <vector>

#include "database/objects/ClusterId.hpp"
#include "track-selection-constraints/TrackCandidateEvaluator.hpp"
#include "track-selection-constraints/TrackMetadata.hpp"

#include "IEngine.hpp"

namespace lms::db
{
    class Session;
}

namespace lms::recommendation
{
    class ClusterEngine : public IEngine
    {
    public:
        ClusterEngine(db::IDb& db);
        ~ClusterEngine() override;
        ClusterEngine(const ClusterEngine&) = delete;
        ClusterEngine& operator=(const ClusterEngine&) = delete;

    private:
        void load() override;

        TrackResults findSimilarTracksFromTrackList(db::TrackListId tracklistId, std::size_t maxCount) const override;
        TrackResults findSimilarTracks(std::span<const db::TrackId> trackIds, std::size_t maxCount) const override;
        TrackResults findTrackSimilarityPath(db::TrackId startTrackId, db::TrackId endTrackId, std::size_t maxCount) const override;
        ReleaseResults findSimilarReleases(db::ReleaseId releaseId, std::size_t maxCount) const override;
        ArtistResults findSimilarArtists(db::ArtistId artistId, core::EnumSet<db::TrackArtistLinkType> linkTypes, std::size_t maxCount) const override;

        TrackResults greedySelect(std::vector<db::TrackId> candidates, std::vector<db::TrackId> selectedTracks, std::size_t maxCount) const;
        void buildTrackMetadata(db::Session& session);
        void buildTrackClusters(db::Session& session);
        void buildReleaseClusters();
        void buildArtistClusters();

        db::IDb& _db;

        TrackMetadataMap _trackMetadata;
        std::unordered_map<db::TrackId, std::vector<db::ClusterId>> _trackClusters;
        std::unordered_map<db::ReleaseId, std::vector<db::ClusterId>> _releaseClusters;
        std::unordered_map<db::ArtistId, std::vector<db::ClusterId>> _artistClusters;
        TrackCandidateEvaluator _trackEvaluator;
    };
} // namespace lms::recommendation
