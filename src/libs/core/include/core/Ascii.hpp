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

#pragma once

#include <string>
#include <string_view>

// Only ASCII characters are considered: meant for keywords, file extensions, tag keys, etc.
// Unlike <cctype>, does not depend on the locale and is fine with negative chars
namespace lms::core::ascii
{
    [[nodiscard]] bool isUpper(char c);
    [[nodiscard]] bool isLower(char c);
    [[nodiscard]] bool isAlpha(char c);
    [[nodiscard]] bool isDigit(char c);
    [[nodiscard]] bool isSpace(char c);

    [[nodiscard]] char toLower(char c);
    [[nodiscard]] char toUpper(char c);

    [[nodiscard]] std::string toLower(std::string_view str);
    void toLower(std::string& str);
    [[nodiscard]] std::string toUpper(std::string_view str);

    [[nodiscard]] bool caseInsensitiveEqual(std::string_view strA, std::string_view strB);
    [[nodiscard]] bool caseInsensitiveContains(std::string_view str, std::string_view strToFind);

    void capitalize(std::string& str);
} // namespace lms::core::ascii
