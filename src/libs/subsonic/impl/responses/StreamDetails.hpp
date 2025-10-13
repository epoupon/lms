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

#include "SubsonicResponse.hpp"

namespace lms::audio
{
    struct AudioProperties;
}

namespace lms::api::subsonic
{
    struct StreamDetails
    {
        std::string protocol;                       // The streaming protocol. Can be http or hls.
        std::string container;                      // The container format (e.g., mp3, flac).
        std::string codec;                          // The audio codec (e.g., mp3, aac, flac).
        std::optional<std::size_t> audioChannels;   // The number of audio channels.
        std::optional<std::size_t> audioBitrate;    // The audio bitrate in kbps.
        std::string audioProfile;                   // The audio profile (e.g., LC, HE-AAC).
        std::optional<std::size_t> audioSamplerate; // The audio sample rate in Hz.
        std::optional<std::size_t> audioBitdepth;   // The audio bit depth in bits.

        auto operator<=>(const StreamDetails&) const = default;
    };

    Response::Node createStreamDetails(const StreamDetails& streamDetails);
} // namespace lms::api::subsonic