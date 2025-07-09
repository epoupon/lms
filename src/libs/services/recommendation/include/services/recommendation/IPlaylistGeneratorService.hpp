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

#include "database/objects/TrackListId.hpp"
#include "services/recommendation/Types.hpp"

namespace lms::db
{
    class IDb;
}

namespace lms::recommendation
{
    class IRecommendationService;
    class IPlaylistGeneratorService
    {
    public:
        virtual ~IPlaylistGeneratorService() = default;

        // extend an existing playlist with similar tracks (but use playlist contraints)
        virtual TrackContainer extendPlaylist(db::TrackListId tracklistId, std::size_t maxCount) const = 0;
    };

    std::unique_ptr<IPlaylistGeneratorService> createPlaylistGeneratorService(db::IDb& db, IRecommendationService& recommendationService);
} // namespace lms::recommendation
