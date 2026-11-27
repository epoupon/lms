/*
 * Copyright (C) 2026 Emeric Poupon
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

#include "core/Ascii.hpp"

#include <algorithm>

namespace lms::core::ascii
{
    bool isUpper(char c)
    {
        return c >= 'A' && c <= 'Z';
    }

    bool isLower(char c)
    {
        return c >= 'a' && c <= 'z';
    }

    bool isAlpha(char c)
    {
        return isUpper(c) || isLower(c);
    }

    bool isDigit(char c)
    {
        return c >= '0' && c <= '9';
    }

    bool isSpace(char c)
    {
        return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
    }

    char toLower(char c)
    {
        return isUpper(c) ? static_cast<char>(c - 'A' + 'a') : c;
    }

    char toUpper(char c)
    {
        return isLower(c) ? static_cast<char>(c - 'a' + 'A') : c;
    }

    std::string toLower(std::string_view str)
    {
        std::string res{ str };
        toLower(res);
        return res;
    }

    void toLower(std::string& str)
    {
        std::transform(std::cbegin(str), std::cend(str), std::begin(str), [](char c) { return toLower(c); });
    }

    std::string toUpper(std::string_view str)
    {
        std::string res{ str };
        std::transform(std::cbegin(res), std::cend(res), std::begin(res), [](char c) { return toUpper(c); });
        return res;
    }

    bool caseInsensitiveEqual(std::string_view strA, std::string_view strB)
    {
        return std::equal(std::cbegin(strA), std::cend(strA), std::cbegin(strB), std::cend(strB),
                          [](char chA, char chB) { return toLower(chA) == toLower(chB); });
    }

    bool caseInsensitiveContains(std::string_view str, std::string_view strToFind)
    {
        if (str.empty() && strToFind.empty())
            return true; // same as std

        const auto it{ std::search(
            std::cbegin(str), std::cend(str),
            std::cbegin(strToFind), std::cend(strToFind),
            [](char chA, char chB) { return toLower(chA) == toLower(chB); }) };
        return (it != std::cend(str));
    }

    void capitalize(std::string& str)
    {
        for (auto it{ std::begin(str) }; it != std::end(str); ++it)
        {
            if (isSpace(*it))
                continue;

            *it = toUpper(*it);

            break;
        }
    }
} // namespace lms::core::ascii
