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

#include "core/String.hpp"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <utility>

#include <Wt/WDate.h>
#include <Wt/WDateTime.h>

namespace lms::core::stringUtils
{
    namespace details
    {
        constexpr std::pair<char, std::string_view> jsEscapeChars[]{
            { '\\', "\\\\" },
            { '\n', "\\n" },
            { '\r', "\\r" },
            { '\t', "\\t" },
            { '"', "\\\"" },
            { '\'', "\\\'" },
        };

        constexpr std::pair<char, std::string_view> jsonEscapeChars[]{
            { '\\', "\\\\" },
            { '\n', "\\n" },
            { '\r', "\\r" },
            { '\t', "\\t" },
            { '"', "\\\"" },
        };

        template<std::size_t N>
        std::string escape(std::string_view str, const std::pair<char, std::string_view> (&charsToEscape)[N])
        {
            std::string escaped;
            escaped.reserve(str.length());

            for (const char c : str)
            {
                auto it{ std::find_if(std::cbegin(charsToEscape), std::cend(charsToEscape), [c](const auto& entry) { return entry.first == c; }) };
                if (it == std::cend(charsToEscape))
                {
                    escaped += c;
                    continue;
                }

                escaped += it->second;
            }

            return escaped;
        }

        template<std::size_t N>
        void writeEscapedString(std::ostream& os, std::string_view str, const std::pair<char, std::string_view> (&charsToEscape)[N])
        {
            for (const char c : str)
            {
                auto itEntry{ std::find_if(std::cbegin(charsToEscape), std::cend(charsToEscape), [=](const auto& entry) { return entry.first == c; }) };
                if (itEntry != std::cend(charsToEscape))
                    os << itEntry->second;
                else
                    os << c;
            }
        }

        template<typename StringType>
        std::string joinStrings(std::span<const StringType> strings, std::string_view delimiter)
        {
            std::string res;
            bool first{ true };

            for (const StringType& str : strings)
            {
                if (!first)
                    res += delimiter;
                res += str;
                first = false;
            }

            return res;
        }

        template<typename StringType>
        std::vector<std::string_view> splitString(std::string_view str, std::span<const StringType> separators)
        {
            std::vector<std::string_view> res;

            size_t currentPos{};
            while (currentPos < str.size())
            {
                size_t nextSeparatorPos{ std::string_view::npos };
                size_t sepLen{};

                for (const std::string_view sep : separators)
                {
                    if (sep.empty())
                        continue;

                    size_t found{ str.find(sep, currentPos) };
                    if (found < nextSeparatorPos)
                    {
                        nextSeparatorPos = found;
                        sepLen = sep.size();
                    }
                }

                if (nextSeparatorPos == std::string_view::npos)
                    break;

                res.push_back(str.substr(currentPos, nextSeparatorPos - currentPos));
                currentPos = nextSeparatorPos + sepLen;
            }

            res.push_back(str.substr(currentPos));
            return res;
        }
    } // namespace details

    template<>
    std::optional<std::string> readAs(std::string_view str)
    {
        return std::string{ str };
    }

    template<>
    std::optional<std::string_view> readAs(std::string_view str)
    {
        return str;
    }

    template<>
    std::optional<bool> readAs(std::string_view str)
    {
        if (str == "1" || stringCaseInsensitiveEqual(str, "true"))
            return true;
        else if (str == "0" || stringCaseInsensitiveEqual(str, "false"))
            return false;

        return std::nullopt;
    }

    std::vector<std::string_view> splitString(std::string_view str, char separator)
    {
        return splitString(str, std::string_view{ &separator, 1 });
    }

    std::vector<std::string_view> splitString(std::string_view str, std::string_view separator)
    {
        return splitString(str, std::span(&separator, 1));
    }

    std::vector<std::string_view> splitString(std::string_view str, std::span<const std::string_view> separators)
    {
        return details::splitString(str, separators);
    }

    std::vector<std::string_view> splitString(std::string_view str, std::span<const std::string> separators)
    {
        return details::splitString(str, separators);
    }

    std::string joinStrings(std::span<const std::string_view> strings, std::string_view delimiter)
    {
        return details::joinStrings(strings, delimiter);
    }

    std::string joinStrings(std::span<const std::string> strings, std::string_view delimiter)
    {
        return details::joinStrings(strings, delimiter);
    }

    std::string joinStrings(std::span<const std::string> strings, char delimiter)
    {
        return details::joinStrings(strings, std::string_view{ &delimiter, 1 });
    }

    std::string joinStrings(std::span<const std::string_view> strings, char delimiter)
    {
        return details::joinStrings(strings, std::string_view{ &delimiter, 1 });
    }

    std::string escapeAndJoinStrings(std::span<const std::string_view> strings, char delimiter, char escapeChar)
    {
        std::string result;
        for (const std::string_view str : strings)
        {
            if (!result.empty())
                result.push_back(delimiter);

            for (char c : str)
            {
                if (c == delimiter || c == escapeChar)
                    result.push_back(escapeChar);

                result.push_back(c);
            }
        }
        return result;
    }

    std::vector<std::string> splitEscapedStrings(std::string_view str, char delimiter, char escapeChar)
    {
        std::vector<std::string> result;
        std::string current;
        bool escaped{};

        for (char c : str)
        {
            if (escaped)
            {
                current.push_back(c);
                escaped = false;
            }
            else
            {
                if (c == delimiter)
                {
                    result.push_back(std::move(current));
                    current.clear();
                }
                else if (c == escapeChar)
                    escaped = true;
                else
                    current.push_back(c);
            }
        }

        if (!current.empty())
            result.push_back(std::move(current));

        return result;
    }

    std::string_view stringTrim(std::string_view str, std::string_view whitespaces)
    {
        std::string_view res;

        const auto strBegin = str.find_first_not_of(whitespaces);
        if (strBegin != std::string_view::npos)
        {
            const auto strEnd{ str.find_last_not_of(whitespaces) };
            const auto strRange{ strEnd - strBegin + 1 };

            res = str.substr(strBegin, strRange);
        }

        return res;
    }

    std::string_view stringTrimEnd(std::string_view str, std::string_view whitespaces)
    {
        return str.substr(0, str.find_last_not_of(whitespaces) + 1);
    }

    std::string stringToLower(std::string_view str)
    {
        std::string res;
        res.reserve(str.size());

        std::transform(std::cbegin(str), std::cend(str), std::back_inserter(res), [](unsigned char c) { return std::tolower(c); });

        return res;
    }

    void stringToLower(std::string& str)
    {
        std::transform(std::cbegin(str), std::cend(str), std::begin(str), [](unsigned char c) { return std::tolower(c); });
    }

    std::string stringToUpper(const std::string& str)
    {
        std::string res;
        res.reserve(str.size());

        std::transform(std::cbegin(str), std::cend(str), std::back_inserter(res), [](char c) { return std::toupper(c); });

        return res;
    }

    std::string bufferToString(std::span<const unsigned char> data)
    {
        std::ostringstream oss;

        for (unsigned char c : data)
        {
            oss << std::setw(2) << std::setfill('0') << std::hex << (int)c;
        }

        return oss.str();
    }

    bool stringCaseInsensitiveEqual(std::string_view strA, std::string_view strB)
    {
        if (strA.size() != strB.size())
            return false;

        for (std::size_t i{}; i < strA.size(); ++i)
        {
            if (std::tolower(strA[i]) != std::tolower(strB[i]))
                return false;
        }

        return true;
    }

    std::string_view::size_type stringCaseInsensitiveContains(std::string_view str, std::string_view strtoFind)
    {
        if (str.empty() && strtoFind.empty())
            return true; // same as std

        const auto it{ std::search(
            std::cbegin(str), std::cend(str),
            std::cbegin(strtoFind), std::cend(strtoFind),
            [](char chA, char chB) { return std::tolower(chA) == std::tolower(chB); }) };
        return (it != std::cend(str));
    }

    void capitalize(std::string& str)
    {
        for (auto it{ std::begin(str) }; it != std::end(str); ++it)
        {
            if (std::isspace(*it))
                continue;

            if (std::isalpha(*it))
                *it = std::toupper(*it);

            break;
        }
    }

    std::string replaceInString(std::string_view str, const std::string& from, const std::string& to)
    {
        std::string res{ str };
        size_t pos = 0;

        while ((pos = res.find(from, pos)) != std::string::npos)
        {
            res.replace(pos, from.length(), to);
            pos += to.length();
        }

        return res;
    }

    std::string jsEscape(std::string_view str)
    {
        return details::escape(str, details::jsEscapeChars);
    }

    void writeJSEscapedString(std::ostream& os, std::string_view str)
    {
        details::writeEscapedString(os, str, details::jsEscapeChars);
    }

    std::string jsonEscape(std::string_view str)
    {
        return details::escape(str, details::jsonEscapeChars);
    }

    void writeJsonEscapedString(std::ostream& os, std::string_view str)
    {
        details::writeEscapedString(os, str, details::jsonEscapeChars);
    }

    std::string escapeString(std::string_view str, std::string_view charsToEscape, char escapeChar)
    {
        std::string res;
        res.reserve(str.size());

        for (const char c : str)
        {
            if (std::any_of(std::cbegin(charsToEscape), std::cend(charsToEscape), [c](char charToEscape) { return c == charToEscape; }))
                res += escapeChar;

            res += c;
        }

        return res;
    }

    std::string unescapeString(std::string_view str, char escapeChar)
    {
        std::string res;
        res.reserve(str.size());

        bool escaped{};

        for (char c : str)
        {
            if (escaped)
            {
                res += c;
                escaped = false;
            }
            else
            {
                if (c == escapeChar)
                    escaped = true;
                else
                    res += c;
            }
        }

        if (escaped)
            res += escapeChar;

        return res;
    }

    bool stringEndsWith(std::string_view str, std::string_view ending)
    {
        if (str.length() < ending.length())
            return false;

        return str.substr(str.length() - ending.length()) == ending;
    }

    std::optional<std::string> stringFromHex(const std::string& str)
    {
        static const char lut[]{ "0123456789ABCDEF" };

        if (str.length() % 2 != 0)
            return std::nullopt;

        std::string res;
        res.reserve(str.length() / 2);

        auto it{ std::cbegin(str) };
        while (it != std::cend(str))
        {
            unsigned val{};

            auto itHigh{ std::lower_bound(std::cbegin(lut), std::cend(lut), std::toupper(*(it++))) };
            auto itLow{ std::lower_bound(std::cbegin(lut), std::cend(lut), std::toupper(*(it++))) };

            if (itHigh == std::cend(lut) || itLow == std::cend(lut))
                return {};

            val = std::distance(std::cbegin(lut), itHigh) << 4;
            val += std::distance(std::cbegin(lut), itLow);

            res.push_back(static_cast<char>(val));
        }

        return res;
    }

    std::string toISO8601String(const Wt::WDateTime& dateTime)
    {
        if (dateTime.isValid())
        {
            // assume UTC
            return dateTime.toString("yyyy-MM-ddThh:mm:ss.zzz", false).toUTF8() + 'Z';
        }

        return "";
    }

    std::string toISO8601String(const Wt::WDate& date)
    {
        if (date.isValid())
        {
            // assume UTC
            return date.toString("yyyy-MM-dd").toUTF8();
        }

        return "";
    }

    Wt::WDateTime fromISO8601String(std::string_view dateTime)
    {
        // assume UTC
        if (!dateTime.empty() && dateTime.back() == 'Z')
            dateTime.remove_suffix(1);

        return Wt::WDateTime::fromString(Wt::WString{ std::string{ dateTime } }, "yyyy-MM-ddThh:mm:ss.zzz");
    }

    std::string formatTimestamp(std::chrono::milliseconds timestamp)
    {
        using namespace std::chrono;

        const auto mins{ duration_cast<minutes>(timestamp).count() };
        timestamp -= duration_cast<milliseconds>(minutes{ mins });
        const auto secs{ duration_cast<seconds>(timestamp).count() };
        timestamp -= duration_cast<milliseconds>(seconds{ secs });
        const auto millis{ timestamp.count() };

        return "[" + std::to_string(mins) + ":" + (secs < 10 ? "0" : "") + std::to_string(secs) + "." + (millis < 100 ? (millis < 10 ? "00" : "0") : "") + std::to_string(millis) + "]";
    }
} // namespace lms::core::stringUtils
