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

#pragma once

#include <chrono>
#include <optional>

#include "core/media/Codec.hpp"
#include "core/media/Container.hpp"

namespace lms::audio
{
    struct AudioProperties
    {
        core::media::Container container;
        core::media::Codec codec;
        std::chrono::milliseconds duration;
        std::optional<unsigned> bitrate; // unset if the file does not tell (thi is not the file-level bitrate!)
        unsigned channelCount;
        unsigned sampleRate;
        std::optional<unsigned> bitsPerSample;
    };
} // namespace lms::audio