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

#include "database/objects/ReleaseId.hpp"

namespace lms::db
{
    class IDb;
}

namespace lms::recommendation::PlaylistGeneratorConstraint
{
    class ConsecutiveReleases : public IConstraint
    {
    public:
        ConsecutiveReleases(db::IDb& db);
        ~ConsecutiveReleases() override = default;
        ConsecutiveReleases(const ConsecutiveReleases&) = delete;
        ConsecutiveReleases& operator=(const ConsecutiveReleases&) = delete;

    private:
        float computeScore(const std::vector<db::TrackId>& trackIds, std::size_t trackIndex) override;

        db::ReleaseId getReleaseId(db::TrackId trackId);

        db::IDb& _db;
    };
} // namespace lms::recommendation::PlaylistGeneratorConstraint
