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
#include <functional>
#include <unordered_map>
#include <vector>

namespace lms::scanner
{
    class IFileScanner;

    class FileScanners
    {
    public:
        void add(std::unique_ptr<IFileScanner> scanner);
        void clear();

        IFileScanner* select(const std::filesystem::path& filePath) const;
        void visit(const std::function<void(const IFileScanner&)>& visitor) const;

    private:
        std::unordered_map<std::filesystem::path, IFileScanner*> _scannerByFile;
        std::unordered_map<std::filesystem::path, IFileScanner*> _scannerByExtension;
        std::vector<std::unique_ptr<IFileScanner>> _fileScanners;
    };
} // namespace lms::scanner
