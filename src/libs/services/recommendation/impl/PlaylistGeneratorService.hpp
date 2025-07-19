/*
 * Copyright (C) 2022 Emeric Poupon
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

#include "services/recommendation/IPlaylistGeneratorService.hpp"
#include "services/recommendation/IRecommendationService.hpp"

#include "playlist-constraints/IConstraint.hpp"

namespace lms::recommendation
{
    class PlaylistGeneratorService : public IPlaylistGeneratorService
    {
    public:
        PlaylistGeneratorService(db::IDb& db, IRecommendationService& recommendationService);

    private:
        TrackContainer extendPlaylist(db::TrackListId tracklistId, std::size_t maxCount) const override;

        TrackContainer getTracksFromTrackList(db::TrackListId tracklistId) const;

        db::IDb& _db;
        IRecommendationService& _recommendationService;
        std::vector<std::unique_ptr<PlaylistGeneratorConstraint::IConstraint>> _constraints;
    };
} // namespace lms::recommendation
