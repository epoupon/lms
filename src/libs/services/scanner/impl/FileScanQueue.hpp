/*
 * Copyright (C) 2024 Emeric Poupon
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

#include <condition_variable>
#include <deque>
#include <filesystem>
#include <mutex>
#include <span>
#include <variant>
#include <vector>

#include "core/IOContextRunner.hpp"
#include "metadata/IParser.hpp"
#include "metadata/Lyrics.hpp"

namespace lms::scanner
{
    struct ImageInfo
    {
        std::size_t height{};
        std::size_t width{};
    };

    using AudioFileScanData = std::unique_ptr<metadata::Track>;
    using ImageFileScanData = std::optional<ImageInfo>;
    using LyricsFileScanData = std::optional<metadata::Lyrics>;
    struct FileScanResult
    {
        std::filesystem::path path;
        std::variant<std::monostate, AudioFileScanData, ImageFileScanData, LyricsFileScanData> scanData;
    };

    class FileScanQueue
    {
    public:
        FileScanQueue(metadata::IParser& parser, std::size_t threadCount, bool& abort);

        std::size_t getThreadCount() const { return _scanContextRunner.getThreadCount(); }

        enum ScanRequestType
        {
            AudioFile,
            ImageFile,
            LyricsFile,
        };
        void pushScanRequest(const std::filesystem::path& path, ScanRequestType type);

        std::size_t getResultsCount() const;
        size_t popResults(std::vector<FileScanResult>& results, std::size_t maxCount);

        void wait(std::size_t maxScanRequestCount = 0); // wait until ongoing scan request count <= maxScanRequestCount

    private:
        AudioFileScanData scanAudioFile(const std::filesystem::path& path);
        ImageFileScanData scanImageFile(const std::filesystem::path& path);
        LyricsFileScanData scanLyricsFile(const std::filesystem::path& path);

        metadata::IParser& _metadataParser;
        boost::asio::io_context _scanContext;
        core::IOContextRunner _scanContextRunner;

        mutable std::mutex _mutex;
        std::size_t _ongoingScanCount{};
        std::deque<FileScanResult> _scanResults;
        std::condition_variable _condVar;
        bool& _abort;
    };

} // namespace lms::scanner
