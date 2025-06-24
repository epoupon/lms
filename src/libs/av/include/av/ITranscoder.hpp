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

#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>

namespace lms::av
{
    struct InputParameters
    {
        std::filesystem::path file;             // Path to the input file
        std::chrono::milliseconds offset{};     // Offset in the input file to start transcoding from
        std::optional<std::size_t> streamIndex; // Index of the stream to be transcoded (select "best" audio stream if not set)
    };

    enum class OutputFormat
    {
        MP3,
        OGG_OPUS,
        MATROSKA_OPUS,
        OGG_VORBIS,
        WEBM_VORBIS,
    };

    struct OutputParameters
    {
        OutputFormat format;
        std::size_t bitrate{ 128'000 };
        bool stripMetadata{ true };
    };

    class ITranscoder
    {
    public:
        virtual ~ITranscoder() = default;

        // non blocking calls
        using ReadCallback = std::function<void(std::size_t nbReadBytes)>;
        virtual void asyncRead(std::byte* buffer, std::size_t bufferSize, ReadCallback callback) = 0;
        virtual std::size_t readSome(std::byte* buffer, std::size_t bufferSize) = 0;

        virtual std::string_view getOutputMimeType() const = 0;
        virtual const OutputParameters& getOutputParameters() const = 0;

        virtual bool finished() const = 0;
    };

    std::unique_ptr<ITranscoder> createTranscoder(const InputParameters& inputParameters, const OutputParameters& outputParameters);
} // namespace lms::av