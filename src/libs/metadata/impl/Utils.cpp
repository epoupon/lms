/*
 * Copyright (C) 2021 Emeric Poupon
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

#include "Utils.hpp"

#include <ctime>
#include <iomanip>
#include <sstream>
#include <string_view>

#include "core/Exception.hpp"

namespace lms::metadata::utils
{
    Wt::WDate parseDate(std::string_view dateStr)
    {
        static constexpr const char* formats[]{
            "%Y-%m-%d",
            "%Y/%m/%d",
        };

        for (const char* format : formats)
        {
            std::tm tm{};
            tm.tm_mon = -1;
            tm.tm_mday = -1;

            std::istringstream ss{ std::string{ dateStr } }; // TODO, remove extra copy here
            ss >> std::get_time(&tm, format);
            if (ss.fail())
                continue;

            if (tm.tm_mday <= 0 || tm.tm_mon < 0)
                continue;

            const Wt::WDate res{
                tm.tm_year + 1900, // tm.tm_year: years since 1900
                tm.tm_mon + 1,     // tm.tm_mon: months since January – [00, 11]
                tm.tm_mday         // tm.tm_mday: day of the month – [1, 31]
            };
            if (!res.isValid())
                continue;

            return res;
        }

        return {};
    }

    std::optional<int> parseYear(std::string_view yearStr)
    {
        // limit to first 4 digit, accept leading '-'
        if (yearStr.empty())
            return std::nullopt;

        int sign;
        if (yearStr.front() == '-')
        {
            sign = -1;
            yearStr.remove_prefix(1);
        }
        else
        {
            sign = 1;
        }

        if (yearStr.empty() || !std::isdigit(yearStr.front()))
            return std::nullopt;

        int result{};
        for (std::size_t i{}; i < yearStr.size() && i < 4; ++i)
        {
            if (!std::isdigit(yearStr[i]))
            {
                break;
            }
            result = result * 10 + (yearStr[i] - '0');
        }

        return result * sign;
    }

    std::string_view readStyleToString(ParserReadStyle readStyle)
    {
        switch (readStyle)
        {
        case ParserReadStyle::Fast:
            return "fast";
        case ParserReadStyle::Average:
            return "average";
        case ParserReadStyle::Accurate:
            return "accurate";
        }

        throw Exception{ "Unknown read style" };
    }

    PerformerArtist extractPerformerAndRole(std::string_view entry)
    {
        std::string_view artistName;
        std::string_view role;

        std::size_t roleBegin{};
        std::size_t roleEnd{};
        std::size_t count{};

        for (std::size_t i{}; i < entry.size(); ++i)
        {
            std::size_t currentIndex{ entry.size() - i - 1 };
            const char c{ entry[currentIndex] };

            if (std::isspace(c))
                continue;

            if (c == ')')
            {
                if (count++ == 0)
                    roleEnd = currentIndex;
            }
            else if (c == '(')
            {
                if (count == 0)
                    break;

                if (--count == 0)
                {
                    roleBegin = currentIndex + 1;
                    role = core::stringUtils::stringTrim(entry.substr(roleBegin, roleEnd - roleBegin));
                    artistName = core::stringUtils::stringTrim(entry.substr(0, currentIndex));
                    break;
                }
            }
            else if (count == 0)
                break;
        }

        if (!roleEnd || !roleBegin)
            artistName = core::stringUtils::stringTrim(entry);

        return PerformerArtist{ Artist{ artistName }, std::string{ role } };
    }
} // namespace lms::metadata::utils