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
#include <functional>

#include "metadata/Types.hpp"

namespace lms::metadata
{
    // using picard internal names
    // see https://picard-docs.musicbrainz.org/en/appendices/tag_mapping.html
    enum class TagType
    {
        AcoustID,
        AcoustIDFingerprint,
        Album,
        AlbumArtist,
        AlbumArtists, // non standard
        AlbumArtistSortOrder,
        AlbumArtistsSortOrder, // non standard
        AlbumSortOrder,
        Arranger,
        Artist,
        ArtistSortOrder,
        Artists,
        ASIN,
        Barcode,
        BPM,
        CatalogNumber,
        Comment,
        Compilation,
        Composer,
        ComposerSortOrder,
        Composers,          // non standard
        ComposersSortOrder, // non standard
        Conductor,
        ConductorSortOrder,  // non standard
        Conductors,          // non standard
        ConductorsSortOrder, // non standard
        Copyright,
        CopyrightURL, // non standard
        Date,
        Director,
        DiscNumber,
        DiscSubtitle,
        EncodedBy,
        EncoderSettings,
        Engineer,
        GaplessPlayback,
        Genre,
        Grouping,
        InitialKey,
        ISRC,
        Language,
        License,
        Lyricist,
        LyricistSortOrder,  // non standard
        Lyricists,          // non standard
        LyricistsSortOrder, // non standard
        // Lyrics, Handled separately
        Media,
        MixDJ,
        Mixer,
        MixerSortOrder,  // non standard
        Mixers,          // non standard
        MixersSortOrder, // non standard
        Mood,
        Movement,
        MovementCount,
        MovementNumber,
        MusicBrainzArtistID,
        MusicBrainzDiscID,
        MusicBrainzOriginalArtistID,
        MusicBrainzOriginalReleaseID,
        MusicBrainzRecordingID,
        MusicBrainzReleaseArtistID,
        MusicBrainzReleaseGroupID,
        MusicBrainzReleaseID,
        MusicBrainzTrackID,
        MusicBrainzWorkID,
        MusicIPFingerprint,
        MusicIPPUID,
        OriginalAlbum,
        OriginalArtist,
        OriginalFilename,
        OriginalReleaseDate,
        OriginalReleaseYear,
        // Performers, Handled separately
        Podcast,
        PodcastURL,
        Producer,
        ProducerSortOrder,  // non standard
        Producers,          // non standard
        ProducersSortOrder, // non standard
        Rating,
        RecordLabel,
        ReleaseCountry,
        ReleaseDate,
        ReleaseStatus,
        ReleaseType,
        Remixer,
        RemixerSortOrder,
        Remixers,
        RemixersSortOrder,
        ReplayGainAlbumGain,
        ReplayGainAlbumPeak,
        ReplayGainAlbumRange,
        ReplayGainReferenceLoudness,
        ReplayGainTrackGain,
        ReplayGainTrackPeak,
        ReplayGainTrackRange,
        Script,
        ShowName,
        ShowNameSortOrder,
        ShowWorkAndMovement,
        Subtitle,
        TotalDiscs,
        TotalTracks,
        TrackNumber,
        TrackTitle,
        TrackTitleSortOrder,
        Website,
        WorkTitle,
        Writer,
    };

    class ITagReader
    {
    public:
        virtual ~ITagReader() = default;

        using TagValueVisitor = std::function<void(std::string_view value)>;
        virtual void visitTagValues(TagType tag, TagValueVisitor visitor) const = 0;
        virtual void visitTagValues(std::string_view tag, TagValueVisitor visitor) const = 0;

        using PerformerVisitor = std::function<void(std::string_view role, std::string_view artist)>;
        virtual void visitPerformerTags(PerformerVisitor visitor) const = 0;

        using LyricsVisitor = std::function<void(std::string_view language, std::string_view lyrics)>;
        virtual void visitLyricsTags(LyricsVisitor visitor) const = 0;

        virtual bool hasEmbeddedCover() const = 0;
        virtual const AudioProperties& getAudioProperties() const = 0;
    };
} // namespace lms::metadata
