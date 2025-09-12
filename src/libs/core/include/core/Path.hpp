/*
 * Copyright (C) 2016 Emeric Poupon
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
#include <string_view>

namespace lms::core::pathUtils
{
    // Check if file's extension is one of provided extensions
    bool hasFileAnyExtension(const std::filesystem::path& file, std::span<const std::filesystem::path> extensions);

    // Check if a path is within a directory (excludeDirFileName is a relative can be used to exclude a whole directory and its subdirectory, must not have parent_path)
    // Caller responsibility to call with normalized paths
    bool isPathInRootPath(const std::filesystem::path& path, const std::filesystem::path& rootPath, const std::filesystem::path* excludeDirFileName = {});

    std::filesystem::path getLongestCommonPath(const std::filesystem::path& path1, const std::filesystem::path& path2);

    template<typename Iterator>
    std::filesystem::path getLongestCommonPath(Iterator first, Iterator last)
    {
        std::filesystem::path longestCommonPath;

        if (first == last)
            return longestCommonPath;

        longestCommonPath = *first++;

        while (first != last)
            longestCommonPath = core::pathUtils::getLongestCommonPath(*first++, longestCommonPath);

        return longestCommonPath;
    }

    // A method that sanitize a file stem, removing any illegal chars
    std::string sanitizeFileStem(std::string_view fileStem);
} // namespace lms::core::pathUtils
