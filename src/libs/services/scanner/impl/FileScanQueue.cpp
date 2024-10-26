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

#include "FileScanQueue.hpp"

#include <fstream>

#include "core/Exception.hpp"
#include "core/IConfig.hpp"
#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"
#include "core/Path.hpp"
#include "image/Exception.hpp"
#include "image/Image.hpp"
#include "metadata/Exception.hpp"

namespace lms::scanner
{
    FileScanQueue::FileScanQueue(metadata::IParser& parser, std::size_t threadCount, bool& abort)
        : _metadataParser{ parser }
        , _scanContextRunner{ _scanContext, threadCount, "FileScan" }
        , _abort{ abort }
    {
    }

    void FileScanQueue::pushScanRequest(const std::filesystem::path& path, ScanRequestType type)
    {
        {
            std::scoped_lock lock{ _mutex };
            _ongoingScanCount += 1;
        }

        _scanContext.post([=, this] {
            if (_abort)
            {
                std::scoped_lock lock{ _mutex };
                _ongoingScanCount -= 1;
            }
            else
            {
                FileScanResult result;
                result.path = path;

                switch (type)
                {
                case ScanRequestType::AudioFile:
                    result.scanData = scanAudioFile(path);
                    break;
                case ScanRequestType::ImageFile:
                    result.scanData = scanImageFile(path);
                    break;
                case ScanRequestType::LyricsFile:
                    result.scanData = scanLyricsFile(path);
                    break;
                }

                {
                    std::scoped_lock lock{ _mutex };

                    _scanResults.emplace_back(std::move(result));
                    _ongoingScanCount -= 1;
                }
            }

            _condVar.notify_all();
        });
    }

    AudioFileScanData FileScanQueue::scanAudioFile(const std::filesystem::path& path)
    {
        LMS_SCOPED_TRACE_OVERVIEW("Scanner", "ScanAudioFile");
        std::unique_ptr<metadata::Track> track;

        try
        {
            track = _metadataParser.parse(path);
        }
        catch (const metadata::Exception& e)
        {
            LMS_LOG(DBUPDATER, INFO, "Failed to parse audio file '" << path.string() << "'");
        }

        return track;
    }

    ImageFileScanData FileScanQueue::scanImageFile(const std::filesystem::path& path)
    {
        LMS_SCOPED_TRACE_OVERVIEW("Scanner", "ScanImageFile");

        std::optional<ImageInfo> optInfo;

        try
        {
            std::unique_ptr<image::IRawImage> rawImage{ image::decodeImage(path) };
            ImageInfo& imageInfo{ optInfo.emplace() };
            imageInfo.width = rawImage->getWidth();
            imageInfo.height = rawImage->getHeight();
        }
        catch (const image::Exception& e)
        {
            LMS_LOG(DBUPDATER, ERROR, "Cannot read image in file '" << path.string() << "': " << e.what());
        }

        return optInfo;
    }

    LyricsFileScanData FileScanQueue::scanLyricsFile(const std::filesystem::path& path)
    {
        LMS_SCOPED_TRACE_OVERVIEW("Scanner", "ScanLyricsFile");

        LyricsFileScanData lyrics;

        try
        {
            std::ifstream ifs{ path.string() };
            if (!ifs)
                LMS_LOG(DBUPDATER, ERROR, "Cannot open file '" << path.string() << "'");
            else
                lyrics = metadata::parseLyrics(ifs);
        }
        catch (const std::exception& e)
        {
            LMS_LOG(DBUPDATER, ERROR, "Cannot read lyrics in file '" << path.string() << "': " << e.what());
        }

        return lyrics;
    }

    std::size_t FileScanQueue::getResultsCount() const
    {
        std::scoped_lock lock{ _mutex };
        return _scanResults.size();
    }

    size_t FileScanQueue::popResults(std::vector<FileScanResult>& results, std::size_t maxCount)
    {
        results.clear();
        results.reserve(maxCount);

        {
            std::scoped_lock lock{ _mutex };

            while (results.size() < maxCount && !_scanResults.empty())
            {
                results.push_back(std::move(_scanResults.front()));
                _scanResults.pop_front();
            }
        }

        return results.size();
    }

    void FileScanQueue::wait(std::size_t maxScanRequestCount)
    {
        LMS_SCOPED_TRACE_OVERVIEW("Scanner", "WaitParseResults");

        std::unique_lock lock{ _mutex };
        _condVar.wait(lock, [=, this] { return _ongoingScanCount <= maxScanRequestCount; });
    }

} // namespace lms::scanner
