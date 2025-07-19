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

#include "IEngine.hpp"

namespace lms::recommendation
{

    class ClusterEngine : public IEngine
    {
    public:
        ClusterEngine(db::IDb& db)
            : _db{ db } {}

        ~ClusterEngine() override = default;
        ClusterEngine(const ClusterEngine&) = delete;
        ClusterEngine(ClusterEngine&&) = delete;
        ClusterEngine& operator=(const ClusterEngine&) = delete;
        ClusterEngine& operator=(ClusterEngine&&) = delete;

    private:
        void load(bool /*forceReload*/, const ProgressCallback& /*progressCallback*/) override {}
        void requestCancelLoad() override {}

        TrackContainer findSimilarTracksFromTrackList(db::TrackListId tracklistId, std::size_t maxCount) const override;
        TrackContainer findSimilarTracks(const std::vector<db::TrackId>& trackIds, std::size_t maxCount) const override;
        ReleaseContainer getSimilarReleases(db::ReleaseId releaseId, std::size_t maxCount) const override;
        ArtistContainer getSimilarArtists(db::ArtistId artistId, core::EnumSet<db::TrackArtistLinkType> linkTypes, std::size_t maxCount) const override;

        db::IDb& _db;
    };

} // namespace lms::recommendation
