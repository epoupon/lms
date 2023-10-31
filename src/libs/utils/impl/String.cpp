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

#include "utils/String.hpp"

#include <algorithm>
#include <iomanip>
#include <utility>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string.hpp>

#include <Wt/WDateTime.h>
#include <Wt/WDate.h>

namespace StringUtils
{

    namespace
    {
        constexpr std::pair<char, std::string_view> jsEscapeChars[]
        {
            { '\\', "\\\\" },
            { '\n', "\\n" },
            { '\r', "\\r" },
            { '\t', "\\t" },
            { '"', "\\\"" },
            { '\'', "\\\'" },
        };

        constexpr std::pair<char, std::string_view> jsonEscapeChars[]
        {
            { '\\', "\\\\" },
            { '\n', "\\n" },
            { '\r', "\\r" },
            { '\t', "\\t" },
            { '"', "\\\"" },
        };

        template <std::size_t N>
        std::string escape(std::string_view str, const std::pair<char, std::string_view>(&charsToEscape)[N])
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

        template <std::size_t N>
        void writeEscapedString(std::ostream& os, std::string_view str, const std::pair<char, std::string_view>(&charsToEscape)[N])
        {
            for (const char c : str)
            {
                auto itEntry{ std::find_if(std::cbegin(charsToEscape), std::cend(charsToEscape), [=](const auto& entry) { return entry.first == c;}) };
                if (itEntry != std::cend(charsToEscape))
                    os << itEntry->second;
                else
                    os << c;
            }
        }
    }

    bool readList(const std::string& str, const std::string& separators, std::list<std::string>& results)
    {
        std::string curStr;

        for (char c : str)
        {
            if (separators.find(c) != std::string::npos) {
                if (!curStr.empty()) {
                    results.push_back(curStr);
                    curStr.clear();
                }
            }
            else {
                if (curStr.empty() && std::isspace(c))
                    continue;

                curStr.push_back(c);
            }
        }

        if (!curStr.empty())
            results.push_back(curStr);

        return !str.empty();
    }

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
        if (str == "1" || str == "true")
            return true;
        else if (str == "0" || str == "false")
            return false;

        return std::nullopt;
    }

    std::vector<std::string> splitStringCopy(std::string_view string, std::string_view separators)
    {
        std::string str{ stringTrim(string, separators) };

        std::vector<std::string> res;
        boost::algorithm::split(res, str, boost::is_any_of(separators), boost::token_compress_on);

        return res;
    }

    std::vector<std::string_view> splitString(std::string_view str, std::string_view separators)
    {
        std::vector<std::string_view> res;

        std::string_view::size_type strBegin{};

        while ((strBegin = str.find_first_not_of(separators, strBegin)) != std::string_view::npos)
        {
            auto strEnd{ str.find_first_of(separators, strBegin + 1) };
            if (strEnd == std::string_view::npos)
            {
                res.push_back(str.substr(strBegin, str.size() - strBegin));
                break;
            }

            res.push_back(str.substr(strBegin, strEnd - strBegin));
            strBegin = strEnd + 1;
        }

        return res;
    }

    std::string joinStrings(const std::vector<std::string>& strings, const std::string& delimiter)
    {
        return boost::algorithm::join(strings, delimiter);
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

        std::transform(std::cbegin(str), std::cend(str), std::back_inserter(res), [](unsigned char c) { return std::tolower(c);});

        return res;
    }

    void stringToLower(std::string& str)
    {
        std::transform(std::cbegin(str), std::cend(str), std::begin(str), [](unsigned char c) { return std::tolower(c);});
    }

    std::string stringToUpper(const std::string& str)
    {
        std::string res;
        res.reserve(str.size());

        std::transform(std::cbegin(str), std::cend(str), std::back_inserter(res), [](char c) { return std::toupper(c);});

        return res;
    }

    std::string bufferToString(const std::vector<unsigned char>& data)
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
        return escape(str, jsEscapeChars);
    }

    void writeJSEscapedString(std::ostream& os, std::string_view str)
    {
        writeEscapedString(os, str, jsEscapeChars);
    }
    
    std::string jsonEscape(std::string_view str)
    {
        return escape(str, jsonEscapeChars);
    }

    void writeJsonEscapedString(std::ostream& os, std::string_view str)
    {
        writeEscapedString(os, str, jsonEscapeChars);
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

    bool stringEndsWith(const std::string& str, const std::string& ending)
    {
        return boost::algorithm::ends_with(str, ending);
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
        // assume UTC
        return dateTime.toString("yyyy-MM-ddThh:mm:ss.zzz", false).toUTF8();
    }

    std::string toISO8601String(const Wt::WDate& date)
    {
        // assume UTC
        return date.toString("yyyy-MM-dd").toUTF8();
    }
} // StringUtils

