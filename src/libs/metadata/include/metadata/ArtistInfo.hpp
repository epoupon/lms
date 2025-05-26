/*
 * Copyright (C) 2025 Emeric Poupon
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

#include <filesystem>
#include <iosfwd>
#include <optional>
#include <span>
#include <string>

#include "core/UUID.hpp"

#include "metadata/Exception.hpp"

namespace lms::metadata
{
    // See:
    // - for the content for the info file: https://kodi.wiki/view/NFO_files/Artists
    // - for the definition of some mb fields: https://musicbrainz.org/doc/Artist
    struct ArtistInfo
    {
        std::string name;
        std::optional<core::UUID> mbid;
        std::string sortName;       // mb
        std::string type;           // mb
        std::string gender;         // mb
        std::string disambiguation; // mb
        std::string biography;
    };

    class ArtistInfoParseException : public Exception
    {
    public:
        using Exception::Exception;
    };

    std::span<const std::filesystem::path> getSupportedArtistInfoFiles();
    ArtistInfo parseArtistInfo(std::istream& is);
} // namespace lms::metadata
