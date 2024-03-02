/*
 * Copyright (C) 2018 Emeric Poupon
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
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <Wt/WDate.h>
#include "utils/UUID.hpp"

namespace MetaData
{
    using Tags = std::map<std::string /* type */, std::vector<std::string> /* values */>;

    // Very simplified version of https://musicbrainz.org/doc/MusicBrainz_Database/Schema

    struct Artist
    {
        std::optional<UUID>			mbid;
        std::string					name;
        std::optional<std::string>	sortName;

        Artist(std::string_view _name) : name{ _name } {}
        Artist(std::optional<UUID> _mbid, std::string_view _name, std::optional<std::string> _sortName) : mbid{ std::move(_mbid) }, name{ _name }, sortName{ std::move(_sortName) } {}

        bool operator<=>(const Artist&) const = default;
    };

    using PerformerContainer = std::map<std::string /*role*/, std::vector<Artist>>;

    struct Release
    {
        std::optional<UUID> 		mbid;
        std::string					name;
        std::string					sortName;
        std::string					artistDisplayName;
        std::vector<Artist>			artists;
        std::optional<std::size_t>	mediumCount;
        std::vector<std::string>    releaseTypes;

        bool operator<=>(const Release&) const = default;
    };

    struct Medium
    {
        std::string					media; // CD, etc.
        std::string					name;
        std::optional<Release>		release;
        std::optional<std::size_t>	position; // in release
        std::optional<std::size_t>  trackCount;
        std::optional<float>        replayGain;

        bool operator<=>(const Medium&) const = default;

        bool isDefault() const
        {
            static Medium defaultMedium;
            return *this == defaultMedium;
        }
    };

    struct Track
    {
        std::optional<UUID>			mbid;
        std::optional<UUID>			recordingMBID;
        std::string					title;
        std::optional<Medium>		medium;
        std::optional<std::size_t>	position; // in medium
        std::vector<std::string>    groupings;
        std::vector<std::string>    genres;
        std::vector<std::string>    moods;
        std::vector<std::string>    labels;
        std::vector<std::string>    languages;
        Tags                        userExtraTags;
        std::chrono::milliseconds 	duration{};
        std::size_t                 bitrate{};
        std::optional<int>          year{};
        Wt::WDate					date;
        std::optional<int>          originalYear{};
        Wt::WDate					originalDate;
        bool						hasCover{};
        std::optional<UUID>			acoustID;
        std::string					copyright;
        std::string					copyrightURL;
        std::optional<float>		replayGain;
        std::string					artistDisplayName;
        std::vector<Artist>			artists;
        std::vector<Artist>			conductorArtists;
        std::vector<Artist>			composerArtists;
        std::vector<Artist>			lyricistArtists;
        std::vector<Artist>			mixerArtists;
        PerformerContainer			performerArtists;
        std::vector<Artist>			producerArtists;
        std::vector<Artist>			remixerArtists;
    };

    class IParser
    {
    public:
        virtual ~IParser() = default;

        virtual std::unique_ptr<Track> parse(const std::filesystem::path& p, bool debug = false) = 0;

        virtual void setUserExtraTags(std::span<const std::string> extraTags) = 0;
        virtual void setArtistTagDelimiters(std::span<const std::string> delimiters) = 0;
        virtual void setDefaultTagDelimiters(std::span<const std::string> delimiters) = 0;
    };

    enum class ParserBackend
    {
        TagLib,
        AvFormat,
    };

    enum class ParserReadStyle
    {
        Fast,
        Average,
        Accurate,
    };
    std::unique_ptr<IParser> createParser(ParserBackend parserBackend, ParserReadStyle parserReadStyle);
} // namespace MetaData
