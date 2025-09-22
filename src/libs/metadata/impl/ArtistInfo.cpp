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

#include "metadata/ArtistInfo.hpp"

#include <pugixml.hpp>

#include "core/ILogger.hpp"
#include "core/LiteralString.hpp"
#include "core/String.hpp"

namespace lms::metadata
{
    namespace
    {
        std::optional<std::string_view> getText(const pugi::xml_node& node, const core::LiteralString& tag)
        {
            std::optional<std::string_view> res;

            if (const pugi::xml_node child{ node.child(tag.c_str()) })
                res = std::string_view{ child.child_value() };

            return res;
        }
    } // namespace

    std::span<const std::filesystem::path> getSupportedArtistInfoFiles()
    {
        static const std::array<std::filesystem::path, 1> files{ "artist.nfo" };
        return files;
    }

    ArtistInfo parseArtistInfo(std::istream& is)
    {
        ArtistInfo artistInfo;
        pugi::xml_document doc;
        pugi::xml_parse_result result{ doc.load(is) };
        if (!result)
        {
            LMS_LOG(METADATA, ERROR, "Cannot read artist info xml: " << result.description());
            throw ArtistInfoParseException{ result.description() };
        }

        const pugi::xml_node artistNode{ doc.child("artist") };
        if (!artistNode)
            throw ArtistInfoParseException{ "No <artist> element found in artist info xml" };

        {
            auto mbid{ getText(artistNode, "musicBrainzArtistID") };
            if (!mbid.has_value())
                mbid = getText(artistNode, "musicbrainzartistid"); // lidarr seems to put this in lowercase
            artistInfo.mbid = core::UUID::fromString(core::stringUtils::stringTrim(mbid.has_value() ? *mbid : ""));
        }

        artistInfo.name = core::stringUtils::stringTrim(getText(artistNode, "name").value_or(""));
        artistInfo.sortName = core::stringUtils::stringTrim(getText(artistNode, "sortname").value_or(""));
        artistInfo.type = core::stringUtils::stringTrim(getText(artistNode, "type").value_or(""));
        artistInfo.gender = core::stringUtils::stringTrim(getText(artistNode, "gender").value_or(""));
        artistInfo.disambiguation = core::stringUtils::stringTrim(getText(artistNode, "disambiguation").value_or(""));
        artistInfo.biography = getText(artistNode, "biography").value_or("");

        return artistInfo;
    }
} // namespace lms::metadata