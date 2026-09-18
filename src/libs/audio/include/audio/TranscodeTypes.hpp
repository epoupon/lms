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

#include "core/media/AudioFormat.hpp"

#include "audio/AudioProperties.hpp"

namespace lms::audio
{
    struct TranscodeInputParameters
    {
        std::filesystem::path filePath;
        AudioProperties audioProperties;    // properties of the audio file
        std::chrono::milliseconds offset{}; // Offset in the audio file to start transcoding from
    };

    using TranscodeOutputFormat = core::media::AudioFormat;

    struct TranscodeOutputParameters
    {
        // Not setting a value here means to use the same value as the input
        std::optional<TranscodeOutputFormat> format;
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