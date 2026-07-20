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

#include "IgnoreRules.hpp"

#include <fnmatch.h>

#include <cassert>
#include <fstream>

namespace lms::scanner
{
    IgnoreRules::IgnoreRules(std::string_view content)
    {
        std::string_view remaining{ content };
        while (!remaining.empty())
        {
            const std::string_view::size_type newlinePos{ remaining.find('\n') };
            std::string_view line{ remaining.substr(0, newlinePos) };
            remaining = (newlinePos != std::string_view::npos) ? remaining.substr(newlinePos + 1) : std::string_view{};

            if (!line.empty() && line.back() == '\r')
                line.remove_suffix(1);

            if (line.empty() || line.front() == '#')
                continue;

            Rule rule{};

            if (line.front() == '!')
            {
                rule.negate = true;
                line.remove_prefix(1);
            }

            if (!line.empty() && line.back() == '/')
            {
                rule.dirOnly = true;
                line.remove_suffix(1);
            }

            // A leading / anchors the pattern to the root.
            bool anchored{};
            if (!line.empty() && line.front() == '/')
            {
                anchored = true;
                line.remove_prefix(1);
            }

            if (line.empty())
                continue;

            rule.pattern = std::string{ line };
            rule.mode = (anchored || rule.pattern.find('/') != std::string::npos) ? MatchMode::FullPath : MatchMode::BasenameOnly;

            _rules.push_back(std::move(rule));
        }
    }

    bool IgnoreRules::isEmpty() const
    {
        return _rules.empty();
    }

    bool IgnoreRules::isIgnored(const std::filesystem::path& relativePath, IsDirectory isDir) const
    {
        assert(relativePath.is_relative());

        if (_rules.empty())
            return false;

        std::filesystem::path currentPath{ relativePath };
        IsDirectory currentIsDir{ isDir };

        while (!currentPath.empty())
        {
            if (matchesRules(currentPath, currentIsDir))
                return true;

            currentPath = currentPath.parent_path();
            currentIsDir = IsDirectory{ true };
        }

        return false;
    }

    bool IgnoreRules::matchesRules(const std::filesystem::path& relativePath, IsDirectory isDir) const
    {
        bool ignored{};
        for (const Rule& rule : _rules)
        {
            if (rule.dirOnly && !isDir.value())
                continue;

            bool matched{};
            switch (rule.mode)
            {
            case MatchMode::BasenameOnly:
                {
                    const std::filesystem::path basename{ relativePath.filename() };
                    matched = (::fnmatch(rule.pattern.c_str(), basename.c_str(), 0) == 0);
                }
                break;
            case MatchMode::FullPath:
                matched = (::fnmatch(rule.pattern.c_str(), relativePath.c_str(), FNM_PATHNAME) == 0);
                break;
            }

            if (matched)
                ignored = !rule.negate;
        }

        return ignored;
    }

    IgnoreRules loadIgnoreRules(const std::filesystem::path& path)
    {
        std::ifstream file{ path };
        if (!file.is_open())
            return IgnoreRules{ {} };

        return IgnoreRules{ std::string{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} } };
    }
} // namespace lms::scanner
