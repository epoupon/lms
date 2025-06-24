/*
 * Copyright (C) 2020 Emeric Poupon
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

#include <chrono>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#define QUOTEME(x) QUOTEME_1(x)
#define QUOTEME_1(x) #x

namespace Wt
{
    class WDate;
    class WDateTime;
} // namespace Wt

namespace lms::core::stringUtils
{
    [[nodiscard]] std::vector<std::string_view> splitString(std::string_view string, char separator);
    [[nodiscard]] std::vector<std::string_view> splitString(std::string_view string, std::string_view separator);
    [[nodiscard]] std::vector<std::string_view> splitString(std::string_view string, std::span<const std::string_view> separators);
    [[nodiscard]] std::vector<std::string_view> splitString(std::string_view string, std::span<const std::string> separators);

    [[nodiscard]] std::string joinStrings(std::span<const std::string> strings, std::string_view delimiter);
    [[nodiscard]] std::string joinStrings(std::span<const std::string_view> strings, std::string_view delimiter);
    [[nodiscard]] std::string joinStrings(std::span<const std::string> strings, char delimiter);
    [[nodiscard]] std::string joinStrings(std::span<const std::string_view> strings, char delimiter);

    [[nodiscard]] std::string escapeAndJoinStrings(std::span<const std::string_view> strings, char delimiter, char escapeChar);
    [[nodiscard]] std::vector<std::string> splitEscapedStrings(std::string_view string, char delimiter, char escapeChar);

    [[nodiscard]] std::string_view stringTrim(std::string_view str, std::string_view whitespaces = " \t\r");
    [[nodiscard]] std::string_view stringTrimEnd(std::string_view str, std::string_view whitespaces = " \t\r");

    [[nodiscard]] std::string stringToLower(std::string_view str);
    void stringToLower(std::string& str);
    [[nodiscard]] std::string stringToUpper(const std::string& str);

    [[nodiscard]] std::string bufferToString(std::span<const unsigned char> data);

    [[nodiscard]] bool stringCaseInsensitiveEqual(std::string_view strA, std::string_view strB);
    [[nodiscard]] std::string_view::size_type stringCaseInsensitiveContains(std::string_view str, std::string_view strtoFind);

    void capitalize(std::string& str);

    template<typename T>
    [[nodiscard]] std::optional<T> readAs(std::string_view str)
    {
        if constexpr (std::is_enum_v<T>)
        {
            using UnderlyingType = std::underlying_type_t<T>;
            std::optional<UnderlyingType> underlyingValue{ readAs<UnderlyingType>(str) };
            if (!underlyingValue)
                return std::nullopt;

            return static_cast<T>(*underlyingValue);
        }
        else
        {
            T res;
            std::istringstream iss{ std::string{ str } };
            iss >> res;
            if (iss.fail())
                return std::nullopt;

            return res;
        }
    }

    template<>
    [[nodiscard]] std::optional<std::string> readAs(std::string_view str);

    template<>
    [[nodiscard]] std::optional<std::string_view> readAs(std::string_view str);

    template<>
    [[nodiscard]] std::optional<bool> readAs(std::string_view str);

    [[nodiscard]] std::string replaceInString(std::string_view str, const std::string& from, const std::string& to);

    [[nodiscard]] std::string jsEscape(std::string_view str);
    [[nodiscard]] std::string jsonEscape(std::string_view str);
    void writeJSEscapedString(std::ostream& os, std::string_view str);
    void writeJsonEscapedString(std::ostream& os, std::string_view str);

    [[nodiscard]] std::string escapeString(std::string_view str, std::string_view charsToEscape, char escapeChar);
    [[nodiscard]] std::string unescapeString(std::string_view str, char escapeChar);

    [[nodiscard]] bool stringEndsWith(std::string_view str, std::string_view ending);

    [[nodiscard]] std::optional<std::string> stringFromHex(const std::string& str);

    [[nodiscard]] std::string toISO8601String(const Wt::WDateTime& dateTime);
    [[nodiscard]] std::string toISO8601String(const Wt::WDate& date);

    [[nodiscard]] Wt::WDateTime fromISO8601String(std::string_view dateTime);

    // to "[minutes:seconds.milliseconds]"
    std::string formatTimestamp(std::chrono::milliseconds timestamp);
} // namespace lms::core::stringUtils