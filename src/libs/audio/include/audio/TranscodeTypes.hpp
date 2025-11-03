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
#include <filesystem>
#include <optional>

// #include "AudioTypes.hpp"

namespace lms::audio
{
    // TODO deprecated
    enum class OutputFormat
    {
        MP3,
        OGG_OPUS,
        MATROSKA_OPUS,
        OGG_VORBIS,
        WEBM_VORBIS,
        FLAC,
    };

    struct TranscodeInputParameters
    {
        std::filesystem::path filePath;
        std::chrono::milliseconds duration{}; // Duration of the audio file
        std::chrono::milliseconds offset{};   // Offset in the audio file to start transcoding from
    };

    struct TranscodeOutputParameters
    {
        std::optional<OutputFormat> format;
        std::optional<unsigned> bitrate;
        std::optional<unsigned> bitsPerSample;
        std::optional<unsigned> channelCount;
        std::optional<unsigned> sampleRate;
        bool stripMetadata{ true };
    };

    struct TranscodeParameters
    {
        TranscodeInputParameters inputParameters;
        TranscodeOutputParameters outputParameters;
    };
} // namespace lms::audio