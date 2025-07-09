/*
 * Copyright (C) 2019 Emeric Poupon
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

#include <memory>

#include "core/EnumSet.hpp"
#include "database/Types.hpp"
#include "database/objects/TrackListId.hpp"
#include "services/recommendation/Types.hpp"

namespace lms::db
{
    class IDb;
}

namespace lms::recommendation
{
    class IEngine
    {
    public:
        virtual ~IEngine() = default;

        virtual void load(bool forceReload, const ProgressCallback& progressCallback = {}) = 0;
        virtual void requestCancelLoad() = 0;

        virtual TrackContainer findSimilarTracksFromTrackList(db::TrackListId tracklistId, std::size_t maxCount) const = 0;
        virtual TrackContainer findSimilarTracks(const std::vector<db::TrackId>& tracksId, std::size_t maxCount) const = 0;
        virtual ReleaseContainer getSimilarReleases(db::ReleaseId releaseId, std::size_t maxCount) const = 0;
        virtual ArtistContainer getSimilarArtists(db::ArtistId artistId, core::EnumSet<db::TrackArtistLinkType> linkTypes, std::size_t maxCount) const = 0;
    };

    std::unique_ptr<IEngine> createEngine(db::IDb& db);

} // namespace lms::recommendation
