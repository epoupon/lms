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

#include "services/recommendation/Types.hpp"

namespace lms::recommendation::PlaylistGeneratorConstraint
{
    class IConstraint
    {
    public:
        virtual ~IConstraint() = default;

        // compute the score of the track at index trackIndex
        // 0: best
        // 1: worst
        // > 1 : violation
        virtual float computeScore(const TrackContainer& trackIds, std::size_t trackIndex) = 0;
    };
} // namespace lms::recommendation::PlaylistGeneratorConstraint
