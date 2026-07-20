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
#include <string>
#include <string_view>
#include <vector>

#include "core/TaggedType.hpp"

namespace lms::scanner
{
    // Parsed representation of a .lmsignore file. See README.md for supported syntax.
    class IgnoreRules
    {
    public:
        explicit IgnoreRules(std::string_view content);

        bool isEmpty() const;

        using IsDirectory = core::TaggedBool<struct IsDirectoryTag>;
        bool isIgnored(const std::filesystem::path& relativePath, IsDirectory isDir) const;

    private:
        enum class MatchMode
        {
            BasenameOnly, // no '/' in pattern: matched against basename only
            FullPath,     // has '/': matched against full relative path
        };

        struct Rule
        {
            bool negate;
            bool dirOnly;
            MatchMode mode;
            std::string pattern;

            bool operator==(const Rule&) const = default;
        };

        bool matchesRules(const std::filesystem::path& relativePath, IsDirectory isDir) const;

        std::vector<Rule> _rules;

    public:
        bool operator==(const IgnoreRules&) const = default;
    };

    // returns an empty IgnoreRules if the file does not exist or cannot be opened.
    IgnoreRules loadIgnoreRules(const std::filesystem::path& path);
} // namespace lms::scanner
