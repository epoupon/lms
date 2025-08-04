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

#include "core/Path.hpp"

#include <array>
#include <unistd.h>

#include "core/ILogger.hpp"
#include "core/String.hpp"

namespace lms::core::pathUtils
{
    bool ensureDirectory(const std::filesystem::path& dir)
    {
        if (std::filesystem::exists(dir))
            return std::filesystem::is_directory(dir);
        else
            return std::filesystem::create_directory(dir);
    }

    bool hasFileAnyExtension(const std::filesystem::path& file, std::span<const std::filesystem::path> supportedExtensions)
    {
        const std::filesystem::path extension{ stringUtils::stringToLower(file.extension().c_str()) };

        return (std::find(std::cbegin(supportedExtensions), std::cend(supportedExtensions), extension) != std::cend(supportedExtensions));
    }

    bool isPathInRootPath(const std::filesystem::path& path, const std::filesystem::path& rootPathArg, const std::filesystem::path* excludeDirFileName)
    {
        std::filesystem::path curPath{ path };
        std::filesystem::path rootPath{ rootPathArg.has_filename() ? rootPathArg : rootPathArg.parent_path() };

        while (true)
        {
            if (excludeDirFileName && !excludeDirFileName->empty())
            {
                assert(!excludeDirFileName->has_parent_path());

                std::error_code ec;
                if (std::filesystem::exists(curPath / *excludeDirFileName, ec))
                    return false;
            }

            if (curPath == rootPath)
                return true;

            if (curPath == curPath.root_path())
                break;

            curPath = curPath.parent_path();
        }

        return false;
    }

    std::filesystem::path getLongestCommonPath(const std::filesystem::path& path1, const std::filesystem::path& path2)
    {
        std::filesystem::path longestCommonPath;

        auto it1{ path1.begin() };
        auto it2{ path2.begin() };

        while (it1 != std::cend(path1) && it2 != std::cend(path2) && *it1 == *it2)
        {
            longestCommonPath /= *it1;
            ++it1;
            ++it2;
        }

        return longestCommonPath;
    }

    std::string sanitizeFileStem(const std::string_view fileStem)
    {
        // Keep UTF8-encoded characters, but skip illegal ASCII characters
        constexpr std::array<unsigned char, 9> illegalChars{ '/', '\\', ':', '*', '?', '"', '<', '>', '|' };
        static_assert(std::all_of(std::begin(illegalChars), std::end(illegalChars), [](unsigned char c) { return c < 128; }), "Illegal characters must be ASCII");

        std::string sanitized;
        sanitized.reserve(fileStem.size());

        for (const char c : fileStem)
        {
            if (std::any_of(std::begin(illegalChars), std::end(illegalChars), [c](char illegalChar) { return c == illegalChar; }))
                continue;

            sanitized.push_back(c);
        }

        return sanitized;
    }
} // namespace lms::core::pathUtils
