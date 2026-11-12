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
#include <cassert>
#include <charconv>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <utility>

#include <utf8proc.h>

#include <Wt/WDate.h>
#include <Wt/WDateTime.h>

namespace lms::core::stringUtils
{
    namespace detail
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
            { '"', "\\\"" },
            { '\b', "\\b" },
            { '\f', "\\f" },
            { '\n', "\\n" },
            { '\r', "\\r" },
            { '\t', "\\t" },
        };

        constexpr std::pair<char, std::string_view> xmlEscapeChars[]{
            { '&', "&amp;" },
            { '<', "&lt;" },
            { '>', "&gt;" },
            { '\'', "&apos;" },
            { '"', "&quot;" },
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

        static std::optional<std::chrono::minutes> getRFC822ZoneOffset(std::string_view zoneStr)
        {
            if (zoneStr == "UT" || zoneStr == "GMT" || zoneStr == "Z") return std::chrono::hours{ 0 };
            if (zoneStr == "EST") return -std::chrono::hours{ 5 };
            if (zoneStr == "EDT") return -std::chrono::hours{ 4 };
            if (zoneStr == "CST") return -std::chrono::hours{ 6 };
            if (zoneStr == "CDT") return -std::chrono::hours{ 5 };
            if (zoneStr == "MST") return -std::chrono::hours{ 7 };
            if (zoneStr == "MDT") return -std::chrono::hours{ 6 };
            if (zoneStr == "PST") return -std::chrono::hours{ 8 };
            if (zoneStr == "PDT") return -std::chrono::hours{ 7 };
            if (zoneStr.size() == 1)
            {
                const char c{ zoneStr[0] };
                if (c == 'A') return -std::chrono::hours{ 1 };
                if (c == 'M') return -std::chrono::hours{ 12 };
                if (c == 'N') return std::chrono::hours{ 1 };
                if (c == 'Y') return std::chrono::hours{ 12 };

                return std::nullopt;
            }

            // (+/-)HHMM
            if (zoneStr[0] != '+' && zoneStr[0] != '-')
                return std::nullopt;

            if (zoneStr.size() != 5)
                return std::nullopt;

            if (!std::all_of(std::cbegin(zoneStr) + 1, std::cend(zoneStr), [](char c) { return std::isdigit(c); }))
                return std::nullopt;

            int hours{};
            const auto [p, ec]{ std::from_chars(zoneStr.data() + 1, zoneStr.data() + 3, hours) };
            if (ec != std::errc())
                return std::nullopt;

            int minutes{};
            const auto [p2, ec2] = std::from_chars(zoneStr.data() + 3, zoneStr.data() + 5, minutes);
            if (ec2 != std::errc())
                return std::nullopt;

            std::chrono::minutes res{ std::chrono::hours{ hours } + std::chrono::minutes{ minutes } };
            if (zoneStr[0] == '-')
                res = -res;
            return res;
        }

        std::optional<unsigned> getRFC822Month(std::string_view monthStr)
        {
            static const std::array<std::string_view, 12> months{ "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
            const std::string_view* str{ std::find(std::cbegin(months), std::cend(months), monthStr) };
            if (str == std::cend(months))
                return {};

            return std::distance(std::cbegin(months), str) + 1;
        }

        // NFKD + full case folding + removal of default ignorables (soft hyphen, ZWJ...), in canonical order
        constexpr auto foldDecomposeOptions{ static_cast<::utf8proc_option_t>(UTF8PROC_STABLE | UTF8PROC_DECOMPOSE | UTF8PROC_COMPAT | UTF8PROC_CASEFOLD | UTF8PROC_IGNORE) };
        constexpr auto foldComposeOptions{ static_cast<::utf8proc_option_t>(UTF8PROC_STABLE | UTF8PROC_COMPOSE) };

        // Accents are only removed from Latin letters: marks are part of the letter in other scripts (Japanese dakuten, Devanagari vowel signs, Cyrillic й)
        constexpr bool isLatinLetter(::utf8proc_int32_t codepoint)
        {
            constexpr std::pair<::utf8proc_int32_t, ::utf8proc_int32_t> latinRanges[]{
                { 0x0041, 0x005A },
                { 0x0061, 0x007A },
                { 0x00C0, 0x02AF },
                { 0x1E00, 0x1EFF },
                { 0x2C60, 0x2C7F },
                { 0xA720, 0xA7FF },
                { 0xAB30, 0xAB6F },
            };

            return std::any_of(std::cbegin(latinRanges), std::cend(latinRanges), [=](const auto& range) { return codepoint >= range.first && codepoint <= range.second; });
        }

        // Latin letters whose accent or stroke is not a separate mark, so decomposition cannot remove it (case folded forms only)
        constexpr std::pair<char32_t, std::string_view> latinTransliterations[]{
            { U'æ', "ae" },
            { U'ð', "d" },
            { U'ø', "o" },
            { U'þ', "th" },
            { U'đ', "d" },
            { U'ħ', "h" },
            { U'ı', "i" },
            { U'ł', "l" },
            { U'œ', "oe" },
            { U'ŧ', "t" },
        };
        // only looked up after a Latin letter
        static_assert(std::all_of(std::cbegin(latinTransliterations), std::cend(latinTransliterations), [](const auto& entry) { return isLatinLetter(static_cast<::utf8proc_int32_t>(entry.first)); }));

        std::string replaceInvalidUtf8(std::string_view str)
        {
            constexpr std::string_view replacementCharacter{ "\xEF\xBF\xBD" };

            std::string res;
            res.reserve(str.size());
            while (!str.empty())
            {
                ::utf8proc_int32_t codepoint;
                const ::utf8proc_ssize_t size{ ::utf8proc_iterate(reinterpret_cast<const ::utf8proc_uint8_t*>(str.data()), static_cast<::utf8proc_ssize_t>(str.size()), &codepoint) };
                if (size < 0)
                {
                    res += replacementCharacter;
                    str.remove_prefix(1);
                }
                else
                {
                    res += str.substr(0, size);
                    str.remove_prefix(size);
                }
            }

            return res;
        }

        std::optional<std::vector<::utf8proc_int32_t>> decomposeForFold(std::string_view str)
        {
            std::vector<::utf8proc_int32_t> codepoints(str.size());
            while (true)
            {
                const ::utf8proc_ssize_t count{ ::utf8proc_decompose(reinterpret_cast<const ::utf8proc_uint8_t*>(str.data()), static_cast<::utf8proc_ssize_t>(str.size()), codepoints.data(), static_cast<::utf8proc_ssize_t>(codepoints.size()), foldDecomposeOptions) };
                if (count == UTF8PROC_ERROR_INVALIDUTF8)
                    return std::nullopt;

                assert(count >= 0);
                const bool fits{ static_cast<std::size_t>(count) <= codepoints.size() };
                codepoints.resize(static_cast<std::size_t>(count));
                if (fits)
                    return codepoints;
                // the buffer was too small: folding expands some code points (ß, ﬃ)
            }
        }
    } // namespace detail

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
        return detail::splitString(str, separators);
    }

    std::vector<std::string_view> splitString(std::string_view str, std::span<const std::string> separators)
    {
        return detail::splitString(str, separators);
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

    std::string_view utf8Truncate(std::string_view str, std::size_t maxBytes)
    {
        std::size_t len{ std::min(maxBytes, str.size()) };
        while (len > 0 && len < str.size() && (static_cast<unsigned char>(str[len]) & 0xC0) == 0x80)
            --len;

        return str.substr(0, len);
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

    std::string foldForComparison(std::string_view str)
    {
        // Same result as the full path, which is much slower
        if (std::all_of(std::cbegin(str), std::cend(str), [](unsigned char c) { return c < 0x80; }))
        {
            std::string res{ str };
            std::transform(std::cbegin(res), std::cend(res), std::begin(res), [](char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; });
            return res;
        }

        const std::optional<std::vector<::utf8proc_int32_t>> codepoints{ detail::decomposeForFold(str) };
        if (!codepoints)
            return foldForComparison(detail::replaceInvalidUtf8(str));

        std::vector<::utf8proc_int32_t> folded;
        folded.reserve(codepoints->size() + 1);

        bool afterLatinLetter{};
        for (const ::utf8proc_int32_t codepoint : *codepoints)
        {
            if (::utf8proc_category(codepoint) == UTF8PROC_CATEGORY_MN)
            {
                if (!afterLatinLetter)
                    folded.push_back(codepoint);
                continue;
            }

            afterLatinLetter = detail::isLatinLetter(codepoint);
            if (afterLatinLetter)
            {
                const auto it{ std::find_if(std::cbegin(detail::latinTransliterations), std::cend(detail::latinTransliterations), [=](const auto& entry) { return static_cast<::utf8proc_int32_t>(entry.first) == codepoint; }) };
                if (it != std::cend(detail::latinTransliterations))
                {
                    for (const char c : it->second)
                        folded.push_back(c);
                    continue;
                }
            }

            folded.push_back(codepoint);
        }

        // utf8proc_reencode writes the UTF-8 result over the code points, followed by a terminating NUL
        const std::size_t count{ folded.size() };
        folded.resize(count + 1);
        const ::utf8proc_ssize_t size{ ::utf8proc_reencode(folded.data(), static_cast<::utf8proc_ssize_t>(count), detail::foldComposeOptions) };
        assert(size >= 0);

        return std::string{ reinterpret_cast<const char*>(folded.data()), static_cast<std::size_t>(size) };
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

    std::string toRomanNumeral(std::size_t n)
    {
        if (n == 0 || n > 3999)
            return {};

        static constexpr struct
        {
            std::size_t val;
            const char* sym;
        } table[]{
            { 1000, "m" },
            { 900, "cm" },
            { 500, "d" },
            { 400, "cd" },
            { 100, "c" },
            { 90, "xc" },
            { 50, "l" },
            { 40, "xl" },
            { 10, "x" },
            { 9, "ix" },
            { 5, "v" },
            { 4, "iv" },
            { 1, "i" }
        };

        std::string res;
        for (const auto& [val, sym] : table)
        {
            while (n >= val)
            {
                res += sym;
                n -= val;
            }
        }

        return res;
    }

    std::string replaceInString(std::string_view str, std::string_view from, std::string_view to)
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
        return detail::escape(str, detail::jsEscapeChars);
    }

    void writeJSEscapedString(std::ostream& os, std::string_view str)
    {
        detail::writeEscapedString(os, str, detail::jsEscapeChars);
    }

    std::string jsonEscape(std::string_view str)
    {
        return detail::escape(str, detail::jsonEscapeChars);
    }

    void writeJsonEscapedString(std::ostream& os, std::string_view str)
    {
        detail::writeEscapedString(os, str, detail::jsonEscapeChars);
    }

    std::string xmlEscape(std::string_view str)
    {
        return detail::escape(str, detail::xmlEscapeChars);
    }

    void writeXmlEscapedString(std::ostream& os, std::string_view str)
    {
        detail::writeEscapedString(os, str, detail::xmlEscapeChars);
    }

    std::string urlEncode(std::string_view str)
    {
        std::ostringstream encoded;
        encoded << std::hex << std::uppercase;

        for (const unsigned char c : str)
        {
            if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
                encoded << c;
            else
                encoded << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(c);
        }

        return encoded.str();
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

    std::optional<std::string> stringFromHex(std::string_view str)
    {
        constexpr char lut[]{ "0123456789ABCDEF" };

        if (str.length() % 2 != 0)
            return std::nullopt;

        std::string res;
        res.reserve(str.length() / 2);

        const char* it{ std::cbegin(str) };
        while (it != std::cend(str))
        {
            unsigned val{};

            const char* itHigh{ std::lower_bound(std::cbegin(lut), std::cend(lut), std::toupper(*(it++))) };
            const char* itLow{ std::lower_bound(std::cbegin(lut), std::cend(lut), std::toupper(*(it++))) };

            if (itHigh == std::cend(lut) || itLow == std::cend(lut))
                return {};

            val = std::distance(std::cbegin(lut), itHigh) << 4;
            val += std::distance(std::cbegin(lut), itLow);

            res.push_back(static_cast<char>(val));
        }

        return res;
    }

    std::string bufferToHexString(std::span<const std::byte> data)
    {
        constexpr char lut[]{ "0123456789ABCDEF" };

        std::string res;
        res.reserve(data.size() * 2);

        for (const std::byte b : data)
        {
            const unsigned value{ std::to_integer<unsigned>(b) };
            res.push_back(lut[(value >> 4) & 0xF]);
            res.push_back(lut[value & 0xF]);
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

    Wt::WDateTime fromRFC822String(std::string_view dateTime)
    {
        // Expect something like "[Sun,] 6 Nov 1994 08:49[:37] GMT"
        if (dateTime.size() > 4 && dateTime[3] == ',')
            dateTime.remove_prefix(5);

        // Extract parts
        const std::vector<std::string_view> subParts{ splitString(dateTime, ' ') };
        if (subParts.size() != 5)
            return {};

        const std::string_view dayStr{ subParts[0] };
        const std::string_view monthStr{ subParts[1] };
        const std::string_view yearStr{ subParts[2] };
        std::string timeStr{ subParts[3] };
        const std::string_view zoneStr{ subParts[4] };
        if (std::count(std::cbegin(timeStr), std::cend(timeStr), ':') == 1)
            timeStr += ":00";

        // Normalize zone
        const std::optional<std::chrono::minutes> offset{ detail::getRFC822ZoneOffset(zoneStr) };
        if (!offset)
            return {};

        std::optional<unsigned> month{ detail::getRFC822Month(monthStr) };
        if (!month)
            return {};

        std::string datetimeStr;
        datetimeStr = dayStr;
        datetimeStr += " ";
        datetimeStr += std::to_string(month.value());
        datetimeStr += " ";
        datetimeStr += yearStr;
        datetimeStr += " ";
        datetimeStr += timeStr;

        const Wt::WDateTime res{ Wt::WDateTime::fromString(Wt::WString{ datetimeStr }, "d M yyyy HH:mm:ss") };
        return res.addSecs(static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(offset.value()).count()));
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
