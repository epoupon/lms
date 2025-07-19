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

#include "FileScanners.hpp"

#include <cassert>

#include "core/String.hpp"

#include "scanners/IFileScanner.hpp"

namespace lms::scanner
{
    void FileScanners::add(std::unique_ptr<IFileScanner> scanner)
    {
        for (const std::filesystem::path& file : scanner->getSupportedFiles())
        {
            [[maybe_unused]] auto [it, inserted]{ _scannerByFile.emplace(file, scanner.get()) };
            assert(inserted);
        }

        for (const std::filesystem::path& extension : scanner->getSupportedExtensions())
        {
            [[maybe_unused]] auto [it, inserted]{ _scannerByExtension.emplace(extension, scanner.get()) };
            assert(inserted);
        }

        _fileScanners.emplace_back(std::move(scanner));
    }

    void FileScanners::clear()
    {
        _fileScanners.clear();
        _scannerByFile.clear();
        _scannerByExtension.clear();
    }

    IFileScanner* FileScanners::select(const std::filesystem::path& filePath) const
    {
        {
            const std::string fileName{ core::stringUtils::stringToLower(filePath.filename().string()) };

            auto itScanner{ _scannerByFile.find(fileName) };
            if (itScanner != std::cend(_scannerByFile))
                return itScanner->second;
        }

        {
            const std::string extension{ core::stringUtils::stringToLower(filePath.extension().string()) };

            auto itScanner{ _scannerByExtension.find(extension) };
            if (itScanner != std::cend(_scannerByExtension))
                return itScanner->second;
        }

        return nullptr;
    }

    void FileScanners::visit(const std::function<void(const IFileScanner&)>& visitor) const
    {
        for (const auto& scanner : _fileScanners)
            visitor(*scanner);
    }

} // namespace lms::scanner
