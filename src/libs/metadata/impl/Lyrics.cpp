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

#include "metadata/Lyrics.hpp"

#include <cassert>
#include <regex>

#include "core/String.hpp"

namespace lms::metadata
{
    std::span<const std::filesystem::path> getSupportedLyricsFileExtensions()
    {
        static const std::array<std::filesystem::path, 2> fileExtensions{ ".lrc", ".txt" }; // TODO handle ".elrc"
        return fileExtensions;
    }

    namespace
    {
        // Parse a single line with a tag like [ar: Artist] and set the appropriate fields in the Lyrics object
        bool parseTag(std::string_view line, Lyrics& lyrics)
        {
            if (line.empty())
                return false;

            if (line.front() != '[' || line.back() != ']') // consider lines are trimmed
                return false;

            const auto separator{ line.find(':') };
            if (separator == std::string_view::npos)
                return false;

            const std::string_view tagType{ core::stringUtils::stringTrim(line.substr(1, separator - 1)) };
            const std::string_view tagValue{ core::stringUtils::stringTrim(line.substr(separator + 1, line.size() - separator - 2)) };

            if (tagType.empty())
                return false;

            // check for timestamps
            if (std::any_of(tagType.begin(), tagType.end(), [](char c) { return std::isdigit(c); }))
                return false;

            if (tagType == "ar")
            {
                lyrics.displayArtist = tagValue;
            }
            else if (tagType == "al")
            {
                lyrics.displayAlbum = tagValue;
            }
            else if (tagType == "ti")
            {
                lyrics.displayTitle = tagValue;
            }
            else if (tagType == "la")
            {
                lyrics.language = tagValue;
            }
            else if (tagType == "offset")
            {
                if (const auto value{ core::stringUtils::readAs<int>(tagValue) })
                    lyrics.offset = std::chrono::milliseconds{ *value };
            }
            // not interrested by other tags like 'duration', 'id', etc.

            return true;
        }

        // Parse timestamps from a line, update the associated times in milliseconds and return the remaining line
        std::string_view extractTimestamps(std::string_view line, std::vector<std::chrono::milliseconds>& timestamps)
        {
            timestamps.clear();
            static const std::regex timeTagRegex{ R"(\[(?:(\d{1,2}):)?(\d{1,2}):(\d{1,2})(?:\.(\d{1,3}))?\])" };
            std::cregex_iterator regexIt(line.begin(), line.end(), timeTagRegex);
            std::cregex_iterator regexEnd;
            std::string_view::size_type offset{};

            while (regexIt != regexEnd)
            {
                std::cmatch match{ *regexIt };
                int hour{ match[1].matched ? std::stoi(match[1].str()) : 0 };
                int minute{ std::stoi(match[2].str()) };
                int second{ std::stoi(match[3].str()) };
                int fractional{ match[4].matched ? std::stoi(match[4].str()) : 0 };

                std::chrono::milliseconds currentTimestamp{ std::chrono::hours{ hour } + std::chrono::minutes{ minute } + std::chrono::seconds{ second } };

                if (match[4].length() == 2) // Centiseconds
                {
                    currentTimestamp += std::chrono::milliseconds{ fractional * 10 };
                }
                else // Milliseconds
                {
                    currentTimestamp += std::chrono::milliseconds{ fractional };
                }

                offset = match[0].second - line.data();
                timestamps.push_back(currentTimestamp);
                ++regexIt;
            }

            return line.substr(offset);
        }
    } // namespace

    // Main function to parse lyrics from an input stream
    Lyrics parseLyrics(std::istream& is)
    {
        Lyrics lyrics;

        enum class State
        {
            None,
            SynchronizedLyrics,
            UnsynchronizedLyrics,
        };
        State currentState{ State::None };

        std::vector<std::chrono::milliseconds> lastTimestamps;
        std::vector<std::chrono::milliseconds> timestamps;
        std::string accumulatedLyrics;

        auto applyAccumulatedLyrics = [&](bool skipTrailingEmptyLines = false) {
            if (lastTimestamps.empty())
                return;

            if (skipTrailingEmptyLines)
                accumulatedLyrics.resize(core::stringUtils::stringTrimEnd(accumulatedLyrics, " \t\r\n").size());

            if (accumulatedLyrics.empty())
                return;

            for (std::chrono::milliseconds timestamp : lastTimestamps)
            {
                std::string& synchronizedLine{ lyrics.synchronizedLines.find(timestamp)->second };
                synchronizedLine += accumulatedLyrics;
            }
            accumulatedLyrics.clear();
        };

        bool firstLine{ true };
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

            std::string_view trimmedLine{ core::stringUtils::stringTrimEnd(line) };

            // Skip comments
            if (!trimmedLine.empty() && trimmedLine.front() == '#')
                continue;

            // Skip empty lines before actual lyrics
            if (currentState == State::None && trimmedLine.empty())
                continue;

            if (parseTag(trimmedLine, lyrics))
                continue;

            const std::string_view lyricsText{ extractTimestamps(trimmedLine, timestamps) };

            // If there are timestamps, add as synchronized lyrics
            if (!timestamps.empty())
            {
                if (currentState == State::UnsynchronizedLyrics)
                    lyrics.unsynchronizedLines.clear(); // choice: discard all lyrics parsed so far

                currentState = State::SynchronizedLyrics;

                applyAccumulatedLyrics();
                for (std::chrono::milliseconds timestamp : timestamps)
                {
                    auto itLine{ lyrics.synchronizedLines.find(timestamp) };
                    if (itLine != std::cend(lyrics.synchronizedLines))
                    {
                        itLine->second.push_back('\n');
                        itLine->second.append(lyricsText);
                    }
                    else
                        lyrics.synchronizedLines.emplace(timestamp, lyricsText);
                }

                lastTimestamps = timestamps;
            }
            else
            {
                if (!lastTimestamps.empty())
                {
                    accumulatedLyrics += '\n';
                    accumulatedLyrics += trimmedLine;
                }
                else
                {
                    assert(currentState != State::SynchronizedLyrics); // should be handled
                    currentState = State::UnsynchronizedLyrics;

                    lyrics.unsynchronizedLines.push_back(std::string{ trimmedLine });
                }
            }
        }
        if (currentState == State::SynchronizedLyrics)
            applyAccumulatedLyrics(true);

        return lyrics;
    }
} // namespace lms::metadata
