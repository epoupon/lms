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

#pragma once

#include <chrono>
#include <filesystem>
#include <iosfwd>
#include <map>
#include <span>
#include <string>
#include <vector>

#include "metadata/Exception.hpp"

namespace lms::metadata
{
    struct Lyrics
    {
        std::string language;
        std::chrono::milliseconds offset{};
        std::string displayArtist;
        std::string displayAlbum;
        std::string displayTitle;

        std::map<std::chrono::milliseconds, std::string> synchronizedLines;
        std::vector<std::string> unsynchronizedLines;
    };

    class LyricsException : public metadata::Exception
    {
    public:
        using metadata::Exception::Exception;
    };

    std::span<const std::filesystem::path> getSupportedLyricsFileExtensions();
    Lyrics parseLyrics(std::istream& is);
} // namespace lms::metadata
