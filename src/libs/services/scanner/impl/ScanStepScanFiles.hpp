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

#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include "metadata/IParser.hpp"

#include "FileScanQueue.hpp"
#include "ScanStepBase.hpp"

namespace lms::scanner
{
    class ScanStepScanFiles : public ScanStepBase
    {
    public:
        ScanStepScanFiles(InitParams& initParams);

    private:
        ScanStep getStep() const override { return ScanStep::ScanFiles; }
        core::LiteralString getStepName() const override { return "Scan files"; }
        void process(ScanContext& context) override;
        void process(ScanContext& context, const ScannerSettings::MediaLibraryInfo& mediaLibrary);

        bool checkAudioFileNeedScan(ScanContext& context, const std::filesystem::path& file, const ScannerSettings::MediaLibraryInfo& libraryInfo);
        bool checkImageFileNeedScan(ScanContext& context, const std::filesystem::path& file);
        bool checkLyricsFileNeedScan(ScanContext& context, const std::filesystem::path& file);

        void processFileScanResults(ScanContext& context, std::span<const FileScanResult> scanResults, const ScannerSettings::MediaLibraryInfo& libraryInfo);
        void processAudioFileScanData(ScanContext& context, const std::filesystem::path& path, const metadata::Track* trackMetadata, const ScannerSettings::MediaLibraryInfo& libraryInfo);
        void processImageFileScanData(ScanContext& context, const std::filesystem::path& path, const ImageInfo* imageInfo, const ScannerSettings::MediaLibraryInfo& libraryInfo);
        void processLyricsFileScanData(ScanContext& context, const std::filesystem::path& path, const metadata::Lyrics* lyrics, const ScannerSettings::MediaLibraryInfo& libraryInfo);

        std::unique_ptr<metadata::IParser> _metadataParser;
        const std::vector<std::string> _extraTagsToParse;

        FileScanQueue _fileScanQueue;
    };
} // namespace lms::scanner
