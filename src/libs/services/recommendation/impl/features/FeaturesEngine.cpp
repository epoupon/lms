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

#include "FeaturesEngine.hpp"

#include <memory>

namespace lms::recommendation
{
    std::unique_ptr<IEngine> createFeaturesEngine(db::IDb& db)
    {
        return std::make_unique<FeaturesEngine>(db);
    }

    FeaturesEngine::FeaturesEngine(db::IDb& db)
        : _db{ db }
    {
    }

    void FeaturesEngine::load(bool forceReload, const ProgressCallback& progressCallback)
    {
    }

    void FeaturesEngine::requestCancelLoad()
    {
    }

    TrackContainer FeaturesEngine::findSimilarTracksFromTrackList(db::TrackListId tracklistId, std::size_t maxCount) const
    {
        return {};
    }

    TrackContainer FeaturesEngine::findSimilarTracks(const std::vector<db::TrackId>& tracksId, std::size_t maxCount) const
    {
        return {};
    }

    ReleaseContainer FeaturesEngine::getSimilarReleases(db::ReleaseId releaseId, std::size_t maxCount) const
    {
        return {};
    }

    ArtistContainer FeaturesEngine::getSimilarArtists(db::ArtistId artistId, core::EnumSet<db::TrackArtistLinkType> linkTypes, std::size_t maxCount) const
    {
        return {};
    }
} // namespace lms::recommendation
