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

#include <filesystem>
#include <memory>
#include <optional>

namespace lms::core
{
    class IChildProcessManager;
    class IResourceHandler;
} // namespace lms::core

namespace lms::transcoding
{
    struct InputParameters
    {
        std::filesystem::path file;             // Path to the input file
        std::chrono::milliseconds duration;     // Offset in the input file to start transcoding from
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

    class ITranscodingService
    {
    public:
        virtual ~ITranscodingService() = default;

        virtual std::unique_ptr<core::IResourceHandler> createResourceHandler(const InputParameters& inputParameters, const OutputParameters& outputParameters, bool estimateContentLength) = 0;
    };

    std::unique_ptr<ITranscodingService> createTranscodingService(core::IChildProcessManager& childProcessManager);
} // namespace lms::transcoding
