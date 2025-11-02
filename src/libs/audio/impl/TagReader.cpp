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

#include "audio/ITagReader.hpp"

namespace lms::audio
{
    core::LiteralString tagTypeToString(TagType type)
    {
        switch (type)
        {
        case TagType::AcoustID:
            return "AcoustID";
        case TagType::AcoustIDFingerprint:
            return "AcoustIDFingerprint";
        case TagType::Advisory:
            return "Advisory";
        case TagType::Album:
            return "Album";
        case TagType::AlbumArtist:
            return "AlbumArtist";
        case TagType::AlbumArtists:
            return "AlbumArtists";
        case TagType::AlbumArtistSortOrder:
            return "AlbumArtistSortOrder";
        case TagType::AlbumArtistsSortOrder:
            return "AlbumArtistsSortOrder";
        case TagType::AlbumComment:
            return "AlbumComment";
        case TagType::AlbumSortOrder:
            return "AlbumSortOrder";
        case TagType::Arranger:
            return "Arranger";
        case TagType::Artist:
            return "Artist";
        case TagType::ArtistSortOrder:
            return "ArtistSortOrder";
        case TagType::Artists:
            return "Artists";
        case TagType::ArtistsSortOrder:
            return "ArtistsSortOrder";
        case TagType::ASIN:
            return "ASIN";
        case TagType::Barcode:
            return "Barcode";
        case TagType::BPM:
            return "BPM";
        case TagType::CatalogNumber:
            return "CatalogNumber";
        case TagType::Comment:
            return "Comment";
        case TagType::Compilation:
            return "Compilation";
        case TagType::Composer:
            return "Composer";
        case TagType::ComposerSortOrder:
            return "ComposerSortOrder";
        case TagType::Composers:
            return "Composers";
        case TagType::ComposersSortOrder:
            return "ComposersSortOrder";
        case TagType::Conductor:
            return "Conductor";
        case TagType::ConductorSortOrder:
            return "ConductorSortOrder";
        case TagType::Conductors:
            return "Conductors";
        case TagType::ConductorsSortOrder:
            return "ConductorsSortOrder";
        case TagType::Copyright:
            return "Copyright";
        case TagType::CopyrightURL:
            return "CopyrightURL";
        case TagType::Date:
            return "Date";
        case TagType::Director:
            return "Director";
        case TagType::DiscNumber:
            return "DiscNumber";
        case TagType::DiscSubtitle:
            return "DiscSubtitle";
        case TagType::EncodedBy:
            return "EncodedBy";
        case TagType::EncoderSettings:
            return "EncoderSettings";
        case TagType::EncodingTime:
            return "EncodingTime";
        case TagType::Engineer:
            return "Engineer";
        case TagType::GaplessPlayback:
            return "GaplessPlayback";
        case TagType::Genre:
            return "Genre";
        case TagType::Grouping:
            return "Grouping";
        case TagType::InitialKey:
            return "InitialKey";
        case TagType::ISRC:
            return "ISRC";
        case TagType::Language:
            return "Language";
        case TagType::Lyricist:
            return "Lyricist";
        case TagType::LyricistSortOrder:
            return "LyricistSortOrder";
        case TagType::Lyricists:
            return "Lyricists";
        case TagType::LyricistsSortOrder:
            return "LyricistsSortOrder";
        case TagType::Media:
            return "Media";
        case TagType::MixDJ:
            return "MixDJ";
        case TagType::Mixer:
            return "Mixer";
        case TagType::MixerSortOrder:
            return "MixerSortOrder";
        case TagType::Mixers:
            return "Mixers";
        case TagType::MixersSortOrder:
            return "MixersSortOrder";
        case TagType::Movement:
            return "Movement";
        case TagType::MovementCount:
            return "MovementCount";
        case TagType::MovementNumber:
            return "MovementNumber";
        case TagType::Mood:
            return "Mood";
        case TagType::MusicBrainzArtistID:
            return "MusicBrainzArtistID";
        case TagType::MusicBrainzArrangerID:
            return "MusicBrainzArrangerID";
        case TagType::MusicBrainzComposerID:
            return "MusicBrainzComposerID";
        case TagType::MusicBrainzConductorID:
            return "MusicBrainzConductorID";
        case TagType::MusicBrainzDirectorID:
            return "MusicBrainzDirectorID";
        case TagType::MusicBrainzDiscID:
            return "MusicBrainzDiscID";
        case TagType::MusicBrainzLyricistID:
            return "MusicBrainzLyricistID";
        case TagType::MusicBrainzProducerID:
            return "MusicBrainzProducerID";
        case TagType::MusicBrainzOriginalArtistID:
            return "MusicBrainzOriginalArtistID";
        case TagType::MusicBrainzRecordingID:
            return "MusicBrainzRecordingID";
        case TagType::MusicBrainzRemixerID:
            return "MusicBrainzRemixerID";
        case TagType::MusicBrainzWorkID:
            return "MusicBrainzWorkID";
        case TagType::Remixer:
            return "Remixer";
        case TagType::Script:
            return "Script";
        case TagType::ShowName:
            return "ShowName";
        case TagType::ShowNameSortOrder:
            return "ShowNameSortOrder";
        case TagType::ShowWorkAndMovement:
            return "ShowWorkAndMovement";
        case TagType::Subtitle:
            return "Subtitle";
        case TagType::TrackNumber:
            return "TrackNumber";
        case TagType::TrackTitle:
            return "TrackTitle";
        case TagType::TrackTitleSortOrder:
            return "TrackTitleSortOrder";
        case TagType::WorkTitle:
            return "WorkTitle";
        case TagType::Writer:
            return "Writer";
        case TagType::License:
            return "License";
        case TagType::MusicBrainzOriginalReleaseID:
            return "MusicBrainzOriginalReleaseID";
        case TagType::MusicBrainzMixerID:
            return "MusicBrainzMixerID";
        case TagType::MusicBrainzReleaseArtistID:
            return "MusicBrainzReleaseArtistID";
        case TagType::MusicBrainzReleaseGroupID:
            return "MusicBrainzReleaseGroupID";
        case TagType::MusicBrainzReleaseID:
            return "MusicBrainzReleaseID";
        case TagType::MusicBrainzTrackID:
            return "MusicBrainzTrackID";
        case TagType::MusicIPFingerprint:
            return "MusicIPFingerprint";
        case TagType::MusicIPPUID:
            return "MusicIPPUID";
        case TagType::OriginalAlbum:
            return "OriginalAlbum";
        case TagType::OriginalArtist:
            return "OriginalArtist";
        case TagType::OriginalFilename:
            return "OriginalFilename";
        case TagType::OriginalReleaseDate:
            return "OriginalReleaseDate";
        case TagType::OriginalReleaseYear:
            return "OriginalReleaseYear";
        case TagType::Podcast:
            return "Podcast";
        case TagType::PodcastURL:
            return "PodcastURL";
        case TagType::Producer:
            return "Producer";
        case TagType::ProducerSortOrder:
            return "ProducerSortOrder";
        case TagType::Producers:
            return "Producers";
        case TagType::ProducersSortOrder:
            return "ProducersSortOrder";
        case TagType::Rating:
            return "Rating";
        case TagType::RecordLabel:
            return "RecordLabel";
        case TagType::ReleaseCountry:
            return "ReleaseCountry";
        case TagType::ReleaseDate:
            return "ReleaseDate";
        case TagType::ReleaseStatus:
            return "ReleaseStatus";
        case TagType::ReleaseType:
            return "ReleaseType";
        case TagType::RemixerSortOrder:
            return "RemixerSortOrder";
        case TagType::Remixers:
            return "Remixers";
        case TagType::RemixersSortOrder:
            return "RemixersSortOrder";
        case TagType::ReplayGainAlbumGain:
            return "ReplayGainAlbumGain";
        case TagType::ReplayGainAlbumPeak:
            return "ReplayGainAlbumPeak";
        case TagType::ReplayGainAlbumRange:
            return "ReplayGainAlbumRange";
        case TagType::ReplayGainReferenceLoudness:
            return "ReplayGainReferenceLoudness";
        case TagType::ReplayGainTrackGain:
            return "ReplayGainTrackGain";
        case TagType::ReplayGainTrackPeak:
            return "ReplayGainTrackPeak";
        case TagType::ReplayGainTrackRange:
            return "ReplayGainTrackRange";
        case TagType::TotalDiscs:
            return "TotalDiscs";
        case TagType::TotalTracks:
            return "TotalTracks";
        case TagType::Website:
            return "Website";

        case TagType::Count:
            break;
        }

        return "";
    }
} // namespace lms::audio