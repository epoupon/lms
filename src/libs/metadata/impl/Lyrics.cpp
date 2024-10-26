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
        static const std::array<std::filesystem::path, 1> fileExtensions{ ".lrc" }; // TODO handle ".txt" and ".elrc"
        return fileExtensions;
    }

    namespace
    {
        std::string_view getSubmatchString(const std::csub_match& submatch)
        {
            assert(submatch.matched);
            return std::string_view{ submatch.first, static_cast<std::string_view::size_type>(submatch.length()) };
        }

        // Parse a single line with ID tags like [ar: Artist] and set the appropriate fields in the Lyrics object
        bool parseIDTag(std::string_view line, Lyrics& lyrics)
        {
            static const std::regex idTagRegex{ R"(^\[([a-zA-Z_]+):(.+?)\])" };
            std::cmatch match;

            if (std::regex_search(line.data(), line.data() + line.size(), match, idTagRegex))
            {
                std::string_view tagType{ getSubmatchString(match[1]) };
                std::string_view tagValue{ core::stringUtils::stringTrim(getSubmatchString(match[2])) };

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

            return false;
        }

        // Parse timestamps from a line and return the associated times in milliseconds
        void extractTimestamps(std::string_view line, std::vector<std::chrono::milliseconds>& timestamps)
        {
            timestamps.clear();
            static const std::regex timeTagRegex{ R"(\[(?:(\d{1,2}):)?(\d{1,2}):(\d{1,2})(?:\.(\d{1,3}))?\])" };
            std::cregex_iterator regexIt(line.begin(), line.end(), timeTagRegex);
            std::cregex_iterator regexEnd;

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

                timestamps.push_back(currentTimestamp);
                ++regexIt;
            }
        }

        // Extract the lyric text from a line, removing any timestamps
        std::string_view extractLyricText(std::string_view line)
        {
            return line.substr(line.find_last_of(']') + 1);
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

        std::string line;
        while (std::getline(is, line))
        {
            std::string_view trimmedLine{ core::stringUtils::stringTrimEnd(line) };

            // Remove potential UTF8 BOM
            constexpr std::string_view utf8BOM{ "\xEF\xBB\xBF" };
            if (trimmedLine.starts_with(utf8BOM))
                trimmedLine = trimmedLine.substr(utf8BOM.size());

            // Skip comments
            if (!trimmedLine.empty() && trimmedLine.front() == '#')
                continue;

            // Skip empty lines before actual lyrics
            if (currentState == State::None && trimmedLine.empty())
                continue;

            if (parseIDTag(trimmedLine, lyrics))
                continue;

            extractTimestamps(trimmedLine, timestamps);

            // If there are timestamps, add as synchronized lyrics
            if (!timestamps.empty())
            {
                if (currentState == State::UnsynchronizedLyrics)
                    lyrics.unsynchronizedLines.clear(); // choice: discard all lyrics parsed so far

                currentState = State::SynchronizedLyrics;

                applyAccumulatedLyrics();
                std::string_view lyricText{ extractLyricText(trimmedLine) };
                for (std::chrono::milliseconds timestamp : timestamps)
                    lyrics.synchronizedLines.emplace(timestamp, lyricText);

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
