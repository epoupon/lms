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
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <boost/tokenizer.hpp>

#include "core/Crc32Calculator.hpp"
#include "core/Exception.hpp"
#include "core/ILogger.hpp"
#include "core/String.hpp"

namespace lms::core::pathUtils
{
    std::uint32_t computeCrc32(const std::filesystem::path& p)
    {
        core::Crc32Calculator crc32;

        std::ifstream ifs{ p.string().c_str(), std::ios_base::binary };
        if (ifs)
        {
            do
            {
                std::array<char, 1024> buffer;

                ifs.read(buffer.data(), buffer.size());
                crc32.processBytes(reinterpret_cast<const std::byte*>(buffer.data()), ifs.gcount());
            } while (ifs);
        }
        else
        {
            LMS_LOG(DBUPDATER, ERROR, "Failed to open file '" << p.string() << "'");
            throw LmsException("Failed to open file '" + p.string() + "'");
        }

        return crc32.getResult();
    }

    bool ensureDirectory(const std::filesystem::path& dir)
    {
        if (std::filesystem::exists(dir))
            return std::filesystem::is_directory(dir);
        else
            return std::filesystem::create_directory(dir);
    }

    Wt::WDateTime getLastWriteTime(const std::filesystem::path& file)
    {
        struct stat sb
        {
        };

        if (stat(file.string().c_str(), &sb) == -1)
            throw LmsException("Failed to get stats on file '" + file.string() + "'");

        return Wt::WDateTime::fromTime_t(sb.st_mtime);
    }

    bool exploreFilesRecursive(const std::filesystem::path& directory, std::function<bool(std::error_code, const std::filesystem::path&)> cb, const std::filesystem::path* excludeDirFileName)
    {
        std::error_code ec;
        std::filesystem::directory_iterator itPath{ directory, std::filesystem::directory_options::follow_directory_symlink, ec };

        if (ec)
        {
            cb(ec, directory);
            return true; // try to continue exploring anyway
        }

        if (excludeDirFileName && !excludeDirFileName->empty())
        {
            const std::filesystem::path excludePath{ directory / *excludeDirFileName };

            if (std::filesystem::exists(excludePath, ec))
            {
                LMS_LOG(DBUPDATER, DEBUG, "Found '" << excludePath.string() << "': skipping directory");
                return true;
            }
        }

        std::filesystem::directory_iterator itEnd;
        while (itPath != itEnd)
        {
            bool continueExploring{ true };

            if (ec)
            {
                continueExploring = cb(ec, *itPath);
            }
            else
            {
                if (std::filesystem::is_regular_file(*itPath, ec))
                {
                    continueExploring = cb(ec, *itPath);
                }
                else if (std::filesystem::is_directory(*itPath, ec))
                {
                    if (!ec)
                        continueExploring = exploreFilesRecursive(*itPath, cb, excludeDirFileName);
                    else
                        continueExploring = cb(ec, *itPath);
                }
            }

            if (!continueExploring)
                return false;

            itPath.increment(ec);
        }

        return true;
    }

    bool hasFileAnyExtension(const std::filesystem::path& file, std::span<const std::filesystem::path> supportedExtensions)
    {
        const std::filesystem::path extension{ stringUtils::stringToLower(file.extension().string()) };

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
} // namespace lms::core::pathUtils
