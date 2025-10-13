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
#include <memory>
#include <optional>

namespace lms
{
    namespace core
    {
        class IChildProcessManager;
        class IResourceHandler;
    } // namespace core

    namespace db
    {
        class IDb;
    }
} // namespace lms

namespace lms::transcoding
{
    struct InputParameters
    {
        std::filesystem::path filePath;
        std::chrono::milliseconds duration{};   // Duration of the audio file
        std::chrono::milliseconds offset{};     // Offset in the audio file to start transcoding from
        std::optional<std::size_t> streamIndex; // Index of the stream to be transcoded (select the "best" audio stream if not set)
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
        std::optional<std::size_t> audioChannels;
        std::optional<std::size_t> sampleRate;
        bool stripMetadata{ true };
    };

    class ITranscodingService
    {
    public:
        virtual ~ITranscodingService() = default;

        virtual std::unique_ptr<core::IResourceHandler> createResourceHandler(const InputParameters& inputParameters, const OutputParameters& outputParameters, bool estimateContentLength) = 0;
    };

    std::unique_ptr<ITranscodingService> createTranscodingService(db::IDb& db, core::IChildProcessManager& childProcessManager);
} // namespace lms::transcoding
