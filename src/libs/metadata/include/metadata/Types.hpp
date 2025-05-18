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
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/PartialDateTime.hpp"
#include "core/UUID.hpp"

#include "Lyrics.hpp"

namespace lms::metadata
{
    using Tags = std::map<std::string /* type */, std::vector<std::string> /* values */>;

    // Very simplified version of https://musicbrainz.org/doc/MusicBrainz_Database/Schema

    struct Artist
    {
        std::optional<core::UUID> mbid;
        std::string name;
        std::optional<std::string> sortName;

        Artist(std::string_view _name)
            : name{ _name } {}
        Artist(std::optional<core::UUID> _mbid, std::string_view _name, std::optional<std::string> _sortName)
            : mbid{ std::move(_mbid) }
            , name{ _name }
            , sortName{ std::move(_sortName) } {}

        auto operator<=>(const Artist&) const = default;
    };

    using PerformerContainer = std::map<std::string /*role*/, std::vector<Artist>>;

    struct Release
    {
        std::optional<core::UUID> mbid;
        std::optional<core::UUID> groupMBID;
        std::string name;
        std::string sortName;
        std::string artistDisplayName;
        std::vector<Artist> artists;
        std::optional<std::size_t> mediumCount;
        std::vector<std::string> labels;
        std::vector<std::string> releaseTypes;
        bool isCompilation{};
        std::string barcode;
        std::string comment;
        std::vector<std::string> countries;

        auto operator<=>(const Release&) const = default;
    };

    struct Medium
    {
        std::string media; // CD, etc.
        std::string name;
        std::optional<Release> release;
        std::optional<std::size_t> position; // in release
        std::optional<std::size_t> trackCount;
        std::optional<float> replayGain;

        auto operator<=>(const Medium&) const = default;

        bool isDefault() const
        {
            static const Medium defaultMedium;
            return *this == defaultMedium;
        }
    };

    struct AudioProperties
    {
        std::size_t bitrate{};
        std::size_t bitsPerSample{};
        std::size_t channelCount{};
        std::chrono::milliseconds duration{};
        std::size_t sampleRate{};
    };

    struct Track
    {
        enum class Advisory
        {
            Unknown,
            Explicit,
            Clean,
        };
        AudioProperties audioProperties;
        std::optional<core::UUID> mbid;
        std::optional<core::UUID> recordingMBID;
        std::string title;
        std::optional<Medium> medium;
        std::optional<std::size_t> position; // in medium
        std::vector<std::string> groupings;
        std::vector<std::string> genres;
        std::vector<std::string> moods;
        std::vector<std::string> languages;
        Tags userExtraTags;
        core::PartialDateTime date;
        std::optional<int> originalYear;
        core::PartialDateTime originalDate;
        std::optional<Advisory> advisory;
        core::PartialDateTime encodingTime;
        std::optional<core::UUID> acoustID;
        std::string copyright;
        std::string copyrightURL;
        std::vector<std::string> comments;
        std::vector<Lyrics> lyrics;
        std::optional<float> replayGain;
        std::string artistDisplayName;
        std::vector<Artist> artists;
        std::vector<Artist> conductorArtists;
        std::vector<Artist> composerArtists;
        std::vector<Artist> lyricistArtists;
        std::vector<Artist> mixerArtists;
        PerformerContainer performerArtists;
        std::vector<Artist> producerArtists;
        std::vector<Artist> remixerArtists;
    };

    struct Image
    {
        // See TagLib types (based on ID3v2 APIC types)
        enum class Type
        {
            // No information
            Unknown,
            // A type not enumerated below
            Other,
            // 32x32 PNG image that should be used as the file icon
            FileIcon,
            // File icon of a different size or format
            OtherFileIcon,
            // Front cover image of the album
            FrontCover,
            // Back cover image of the album
            BackCover,
            // Inside leaflet page of the album
            LeafletPage,
            // Image from the album itself
            Media,
            // Picture of the lead artist or soloist
            LeadArtist,
            // Picture of the artist or performer
            Artist,
            // Picture of the conductor
            Conductor,
            // Picture of the band or orchestra
            Band,
            // Picture of the composer
            Composer,
            // Picture of the lyricist or text writer
            Lyricist,
            // Picture of the recording location or studio
            RecordingLocation,
            // Picture of the artists during recording
            DuringRecording,
            // Picture of the artists during performance
            DuringPerformance,
            // Picture from a movie or video related to the track
            MovieScreenCapture,
            // Picture of a large, coloured fish
            ColouredFish,
            // Illustration related to the track
            Illustration,
            // Logo of the band or performer
            BandLogo,
            // Logo of the publisher (record company)
            PublisherLogo
        };

        Type type{ Type::Unknown };
        std::string mimeType{ "application/octet-stream" };
        std::string description;
        std::span<const std::byte> data;
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

    struct SortByLengthDesc
    {
        bool operator()(const std::string& a, const std::string& b) const
        {
            if (a.length() != b.length())
                return a.length() > b.length();
            return a < b; // Break ties using lexicographical order
        }
    };

    using WhiteList = std::set<std::string, SortByLengthDesc>;
    struct AudioFileParserParameters
    {
        ParserBackend backend{ ParserBackend::TagLib };
        ParserReadStyle readStyle{ ParserReadStyle::Average };
        std::vector<std::string> artistTagDelimiters;
        WhiteList artistsToNotSplit;
        std::vector<std::string> defaultTagDelimiters;
        std::vector<std::string> userExtraTags;
        bool debug{};
    };
} // namespace lms::metadata