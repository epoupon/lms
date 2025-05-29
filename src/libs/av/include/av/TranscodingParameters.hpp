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

#include "Types.hpp"
#include "core/XxHash3.hpp"

#include <chrono>
#include <filesystem>
#include <optional>

namespace lms::av::transcoding
{
    struct InputParameters
    {
        std::filesystem::path trackPath;
        std::chrono::milliseconds duration; // used to estimate content length

        uint64_t hash() const
        {
            const auto str{ trackPath.string() };
            return core::xxHash3_64({
                reinterpret_cast<const std::byte*>(str.data()),
                str.size()
            })
                ^
            static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
        }
    };

    enum class OutputFormat
    {
        MP3,
        OGG_OPUS,
        MATROSKA_OPUS,
        OGG_VORBIS,
        WEBM_VORBIS,
    };

    static inline std::string_view formatToMimetype(OutputFormat format)
    {
        switch (format)
        {
        case OutputFormat::MP3:
            return "audio/mpeg";
        case OutputFormat::OGG_OPUS:
            return "audio/opus";
        case OutputFormat::MATROSKA_OPUS:
            return "audio/x-matroska";
        case OutputFormat::OGG_VORBIS:
            return "audio/ogg";
        case OutputFormat::WEBM_VORBIS:
            return "audio/webm";
        }

        throw Exception{ "Invalid encoding" };
    }

    struct OutputParameters
    {
        OutputFormat format;
        std::size_t bitrate{ 128'000 };
        std::optional<std::size_t> stream; // Id of the stream to be transcoded (auto detect by default)
        std::chrono::milliseconds offset{ 0 };
        bool stripMetadata{ true };

        uint64_t hash() const
        {
            return static_cast<uint64_t>(format) ^ bitrate ^ (stream ? *stream : UINT64_MAX) ^ static_cast<uint64_t>(stripMetadata);
        }
    };
} // namespace lms::av::transcoding
