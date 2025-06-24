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

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include "core/ILogger.hpp"
#include "core/String.hpp"

namespace lms::metadata
{
    std::span<const std::filesystem::path> getSupportedArtistInfoFiles()
    {
        static const std::array<std::filesystem::path, 1> files{ "artist.nfo" };
        return files;
    }

    ArtistInfo parseArtistInfo(std::istream& is)
    {
        try
        {
            ArtistInfo artistInfo;

            boost::property_tree::ptree root;
            boost::property_tree::read_xml(is, root);

            const auto& artistNode{ root.get_child("artist") };

            artistInfo.mbid = core::UUID::fromString(core::stringUtils::stringTrim(artistNode.get_optional<std::string>("musicBrainzArtistID").value_or("")));
            artistInfo.name = core::stringUtils::stringTrim(artistNode.get_optional<std::string>("name").value_or(""));
            artistInfo.sortName = core::stringUtils::stringTrim(artistNode.get_optional<std::string>("sortname").value_or(""));
            artistInfo.type = core::stringUtils::stringTrim(artistNode.get_optional<std::string>("type").value_or(""));
            artistInfo.gender = core::stringUtils::stringTrim(artistNode.get_optional<std::string>("gender").value_or(""));
            artistInfo.disambiguation = core::stringUtils::stringTrim(artistNode.get_optional<std::string>("disambiguation").value_or(""));
            artistInfo.biography = artistNode.get_optional<std::string>("biography").value_or("");

            return artistInfo;
        }
        catch (boost::property_tree::ptree_error& error)
        {
            LMS_LOG(METADATA, ERROR, "Cannot read artist xml info: " << error.what());
            throw ArtistInfoParseException{ error.what() };
        }
    }
} // namespace lms::metadata