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

#include <span>

#include "core/EnumSet.hpp"

#include "database/objects/TrackListId.hpp"
#include "database/objects/Types.hpp"
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

        virtual void load() = 0;

        virtual TrackResults findSimilarTracksFromTrackList(db::TrackListId tracklistId, std::size_t maxCount) const = 0;
        virtual TrackResults findSimilarTracks(std::span<const db::TrackId> tracksId, std::size_t maxCount) const = 0;
        virtual ReleaseResults findSimilarReleases(db::ReleaseId releaseId, std::size_t maxCount) const = 0;
        virtual ArtistResults findSimilarArtists(db::ArtistId artistId, core::EnumSet<db::TrackArtistLinkType> linkTypes, std::size_t maxCount) const = 0;
        virtual TrackResults findTrackSimilarityPath(db::TrackId startTrackId, db::TrackId endTrackId, std::size_t maxCount) const = 0;
    };
} // namespace lms::recommendation
