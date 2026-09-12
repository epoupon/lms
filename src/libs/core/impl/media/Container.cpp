/*
 * Copyright (C) 2025 Emeric Poupon
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

#include "core/media/Container.hpp"

#include <algorithm>
#include <array>

namespace lms::core::media
{
    namespace
    {
        constexpr std::array allContainers{
            Container::AIFF,
            Container::APE,
            Container::ASF,
            Container::DSF,
            Container::FLAC,
            Container::MP4,
            Container::MPC,
            Container::MPEG,
            Container::Ogg,
            Container::Shorten,
            Container::TrueAudio,
            Container::WAV,
            Container::WavPack,
        };
    } // namespace

    void visitContainers(const std::function<void(Container)>& visitor)
    {
        std::for_each(std::cbegin(allContainers), std::cend(allContainers), visitor);
    }

    core::LiteralString containerToString(Container type)
    {
        switch (type)
        {
        case Container::AIFF:
            return "AIFF";
        case Container::APE:
            return "APE";
        case Container::ASF:
            return "ASF";
        case Container::DSF:
            return "DSF";
        case Container::FLAC:
            return "FLAC";
        case Container::MP4:
            return "MP4";
        case Container::MPC:
            return "MPC";
        case Container::MPEG:
            return "MPEG";
        case Container::Ogg:
            return "Ogg";
        case Container::Shorten:
            return "Shorten";
        case Container::TrueAudio:
            return "TrueAudio";
        case Container::WAV:
            return "WAV";
        case Container::WavPack:
            return "WavPack";
        }

        return "Unknown";
    }
} // namespace lms::core::media