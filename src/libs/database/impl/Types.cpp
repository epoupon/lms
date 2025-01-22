/*
 * Copyright (C) 2015 Emeric Poupon
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

#include "database/Types.hpp"

#include <set>

namespace lms::db
{
    static const std::set<Bitrate> allowedAudioBitrates{
        64000,
        96000,
        128000,
        192000,
        320000,
    };

    void visitAllowedAudioBitrates(std::function<void(Bitrate)> func)
    {
        for (Bitrate bitrate : allowedAudioBitrates)
            func(bitrate);
    }

    bool isAudioBitrateAllowed(Bitrate bitrate)
    {
        return allowedAudioBitrates.find(bitrate) != std::cend(allowedAudioBitrates);
    }
} // namespace lms::db
