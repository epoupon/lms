/*
 * Copyright (C) 2023 Emeric Poupon
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
#include <string>
#include <vector>

#include "metadata/IParser.hpp"
#include "utils/IOContextRunner.hpp"
#include "ScanStepBase.hpp"

namespace Scanner
{
    class ScanStepScanFiles : public ScanStepBase
    {
    public:
        ScanStepScanFiles(InitParams& initParams);

    private:
        ScanStep getStep() const override { return ScanStep::ScanningFiles; }
        std::string_view getStepName() const override { return "Scanning files"; }
        void process(ScanContext& context) override;

        bool checkFileNeedScan(ScanContext& context, const std::filesystem::path& file, const ScannerSettings::MediaLibraryInfo& libraryInfo);
        struct MetaDataScanResult
        {
            std::filesystem::path path;
            std::optional<MetaData::Track> trackMetaData;
        };
        void processMetaDataScanResults(ScanContext& context, std::span<const std::unique_ptr<MetaDataScanResult>> scanResults, const ScannerSettings::MediaLibraryInfo& libraryInfo);
        void processFileMetaData(ScanContext& context, const std::filesystem::path& file, const MetaData::Track& trackMetadata, const ScannerSettings::MediaLibraryInfo& libraryInfo);

        std::unique_ptr<MetaData::IParser>  _metadataParser;
        const std::vector<std::string>      _extraTagsToParse{ "GENRE", "MOOD", "LANGUAGE", "ALBUMGROUPING" };

        class MetadataScanQueue
        {
            public:
                MetadataScanQueue(MetaData::IParser& parser, std::size_t threadCount);

                std::size_t getThreadCount() const { return _scanContextRunner.getThreadCount(); }

                void pushScanRequest(const std::filesystem::path path);

                std::size_t getResultsCount() const;
                size_t popResults(std::vector<std::unique_ptr<MetaDataScanResult>>& results, std::size_t maxCount);

                void wait(std::size_t maxScanRequestCount = 0); // wait until ongoing scan request count <= maxScanRequestCount

            private:
                MetaData::IParser& _metadataParser;
                boost::asio::io_context _scanContext;
                IOContextRunner _scanContextRunner;

                mutable std::mutex _mutex ;
                std::size_t _ongoingScanCount{};
                std::deque<std::unique_ptr<MetaDataScanResult>> _scanResults;
                std::condition_variable _condVar;
        };
        MetadataScanQueue _metadataScanQueue;

        std::deque<std::unique_ptr<MetaDataScanResult>> _metaDataScanResults;
    };
}
