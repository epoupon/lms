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

#include <filesystem>
#include <span>

#include "core/LiteralString.hpp"

#include "FileToScan.hpp"

namespace lms::scanner
{
    class IFileScanOperation;

    class IFileScanner
    {
    public:
        virtual ~IFileScanner() = default;

        virtual core::LiteralString getName() const = 0;
        virtual std::span<const std::filesystem::path> getSupportedFiles() const = 0;
        virtual std::span<const std::filesystem::path> getSupportedExtensions() const = 0;
        virtual bool needsScan(const FileToScan& file) const = 0;
        virtual std::unique_ptr<IFileScanOperation> createScanOperation(FileToScan&& fileToScan) const = 0;
    };
} // namespace lms::scanner