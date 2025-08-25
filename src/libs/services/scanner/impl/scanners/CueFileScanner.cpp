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

#include "CueFileScanner.hpp"

#include "CueFileScanOperation.hpp"

namespace lms::scanner
{
    CueFileScanner::~CueFileScanner() = default;

    core::LiteralString CueFileScanner::getName() const
    {
        return ".cue file scanner";
    }

    std::span<const std::filesystem::path> CueFileScanner::getSupportedFiles() const
    {
        return {};
    }

    std::span<const std::filesystem::path> CueFileScanner::getSupportedExtensions() const
    {
        static const std::vector<std::filesystem::path> ret{ ".cue" };
        return ret;
    }

    bool CueFileScanner::needsScan(const FileToScan& file) const
    {
        return true;
    }

    std::unique_ptr<IFileScanOperation> CueFileScanner::createScanOperation(FileToScan&& fileToScan) const
    {
        return std::make_unique<CueFileScanOperation>(std::move(fileToScan), getDb(), getScannerSettings(), getMetadataParser());
    }

} // namespace lms::scanner
