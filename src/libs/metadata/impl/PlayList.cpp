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

#include "metadata/PlayList.hpp"

#include <array>
#include <string_view>

#include "core/String.hpp"

namespace lms::metadata
{
    std::span<const std::filesystem::path> getSupportedPlayListFileExtensions()
    {
        static const std::array<std::filesystem::path, 2> fileExtensions{ ".m3u", ".m3u8" };
        return fileExtensions;
    }

    namespace
    {
        struct Comment
        {
            std::string_view directive;
            std::string_view parameter;
        };

        std::optional<Comment> parseComment(std::string_view line)
        {
            if (line.empty() || line.front() != '#')
                return std::nullopt;

            Comment comment;
            const std::string_view::size_type parameterSeparator{ line.find(':') };
            if (parameterSeparator == std::string_view::npos)
            {
                comment.directive = line;
            }
            else
            {
                comment.directive = line.substr(0, parameterSeparator + 1);
                comment.parameter = line.substr(parameterSeparator + 1);
            };

            return comment;
        }
    } // namespace

    PlayList parsePlayList(std::istream& is)
    {
        bool firstLine{ true };
        PlayList playlist;

        std::string line;
        while (std::getline(is, line))
        {
            // Remove potential UTF8 BOM
            if (firstLine)
            {
                firstLine = false;
                constexpr std::string_view utf8BOM{ "\xEF\xBB\xBF" };
                if (line.starts_with(utf8BOM))
                    line.erase(0, utf8BOM.size());
            }

            const std::string_view trimmedLine{ core::stringUtils::stringTrim(line) };
            if (trimmedLine.empty())
                continue;

            // Don't enforce #EXTM3U as first line: be permissive
            if (const std::optional<Comment> comment{ parseComment(trimmedLine) })
            {
                if (comment->directive == "#PLAYLIST:")
                    playlist.name = comment->parameter;

                continue;
            }

            // filter out URI = scheme ":" ["//" authority] path ["?" query] ["#" fragment]
            // Consider an entry with a ':' is actually an url, as filenames are not supposed to have ':' on windows
            if (trimmedLine.find(':') != std::string_view::npos)
                continue;

            const std::filesystem::path path{ std::cbegin(trimmedLine), std::cend(trimmedLine) };
            playlist.files.emplace_back(path.lexically_normal());
        }

        return playlist;
    }
} // namespace lms::metadata
