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

#include "FileScanQueue.hpp"
#include "ScanStepBase.hpp"

namespace lms::scanner
{
    class IFileScanner;
    class MediaLibraryInfo;

    class ScanStepScanFiles : public ScanStepBase
    {
    public:
        ScanStepScanFiles(InitParams& initParams);

    private:
        ScanStep getStep() const override { return ScanStep::ScanFiles; }
        core::LiteralString getStepName() const override { return "Scan files"; }
        void process(ScanContext& context) override;
        void process(ScanContext& context, const MediaLibraryInfo& mediaLibrary);
        void processFileScanResults(ScanContext& context, std::span<std::unique_ptr<IFileScanOperation>> scanOperations);

        FileScanQueue _fileScanQueue;
        std::unordered_map<std::filesystem::path, IFileScanner*> _scannerByExtension;
    };
} // namespace lms::scanner
