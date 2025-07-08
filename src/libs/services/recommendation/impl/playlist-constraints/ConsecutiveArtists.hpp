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

#include "IConstraint.hpp"

namespace lms::db
{
    class IDb;
}

namespace lms::recommendation::PlaylistGeneratorConstraint
{
    class ConsecutiveArtists : public IConstraint
    {
    public:
        ConsecutiveArtists(db::IDb& db);
        ~ConsecutiveArtists() override = default;
        ConsecutiveArtists(const ConsecutiveArtists&) = delete;
        ConsecutiveArtists& operator=(const ConsecutiveArtists&) = delete;

    private:
        float computeScore(const TrackContainer& trackIds, std::size_t trackIndex) override;
        ArtistContainer getArtists(db::TrackId trackId);

        db::IDb& _db;
    };
} // namespace lms::recommendation::PlaylistGeneratorConstraint
