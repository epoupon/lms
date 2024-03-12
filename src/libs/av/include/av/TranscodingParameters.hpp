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

#pragma once

#include <chrono>
#include <filesystem>
#include <optional>

#include "Types.hpp"

namespace lms::av::transcoding
{
    struct InputParameters
    {
        std::filesystem::path trackPath;
        std::chrono::milliseconds duration; // used to estimate content length
    };

    enum class OutputFormat
    {
        MP3,
        OGG_OPUS,
        MATROSKA_OPUS,
        OGG_VORBIS,
        WEBM_VORBIS,
    };

    std::string_view toMimetype(OutputFormat format);

    struct OutputParameters
    {
        OutputFormat                format;
        std::size_t                 bitrate{ 128000 };
        std::optional<std::size_t>  stream; // Id of the stream to be transcoded (auto detect by default)
        std::chrono::milliseconds   offset{ 0 };
        bool                        stripMetadata{ true };
    };
} // namespace lms::av::Transcoding

