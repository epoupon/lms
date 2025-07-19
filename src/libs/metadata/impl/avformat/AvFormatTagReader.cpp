/*
 * Copyright (C) 2013 Emeric Poupon
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

#include "AvFormatTagReader.hpp"

#include "av/Exception.hpp"
#include "av/IAudioFile.hpp"
#include "core/ILogger.hpp"
#include "core/String.hpp"
#include "metadata/Exception.hpp"

namespace lms::metadata::avformat
{
    namespace
    {
        // Mapping to internal avformat names and/or common alternative custom names
        static const std::unordered_map<TagType, std::vector<std::string>> avFormatTagMapping{
            { TagType::AcoustID, { "ACOUSTID_ID", "ACOUSTID ID" } },
            { TagType::Advisory, { "ITUNESADVISORY" } },
            { TagType::Album, { "ALBUM", "TALB", "WM/ALBUMTITLE" } },
            { TagType::AlbumArtist, { "ALBUMARTIST", "ALBUM_ARTIST" } },
            { TagType::AlbumArtistSortOrder, { "ALBUMARTISTSORT", "TSO2" } },
            { TagType::AlbumArtists, { "ALBUMARTISTS" } },
            { TagType::AlbumArtistsSortOrder, { "ALBUMARTISTSSORT" } },
            { TagType::AlbumComment, { "ALBUMCOMMENT", "MUSICBRAINZ_ALBUMCOMMENT, MUSICBRAINZ ALBUM COMMENT", "MUSICBRAINZ/ALBUM COMMENT", "ALBUMVERSION", "VERSION" } },
            { TagType::AlbumSortOrder, { "ALBUMSORT", "ALBUM-SORT" } },
            { TagType::Arranger, { "ARRANGER" } },
            { TagType::Artist, { "ARTIST" } },
            { TagType::ArtistSortOrder, { "ARTISTSORT", "ARTIST-SORT", "WM/ARTISTSORTORDER" } },
            { TagType::Artists, { "ARTISTS", "WM/ARTISTS" } },
            { TagType::ASIN, { "ASIN" } },
            { TagType::Barcode, { "BARCODE", "WM/BARCODE" } },
            { TagType::BPM, { "BPM" } },
            { TagType::CatalogNumber, { "CATALOGNUMBER", "WM/CATALOGNO" } },
            { TagType::Comment, { "COMMENT" } },
            { TagType::Compilation, { "COMPILATION", "TCMP" } },
            { TagType::Composer, { "COMPOSER" } },
            { TagType::Composers, { "COMPOSERS" } },
            { TagType::ComposerSortOrder, { "COMPOSERSORT", "TSOC" } },
            { TagType::ComposersSortOrder, { "COMPOSERSSORT" } },
            { TagType::Conductor, { "CONDUCTOR" } },
            { TagType::ConductorSortOrder, { "CONDUCTORSORT" } },
            { TagType::Conductors, { "CONDUCTORS" } },
            { TagType::ConductorsSortOrder, { "CONDUCTORSSORT" } },
            { TagType::Copyright, { "COPYRIGHT" } },
            { TagType::CopyrightURL, { "COPYRIGHTURL" } },
            { TagType::Date, { "DATE", "YEAR", "WM/YEAR" } },
            { TagType::Director, { "DIRECTOR" } },
            { TagType::DiscNumber, { "TPOS", "DISC", "DISK", "DISCNUMBER", "WM/PARTOFSET" } },
            { TagType::DiscSubtitle, { "TSST", "DISCSUBTITLE", "SETSUBTITLE" } },
            { TagType::EncodedBy, { "ENCODEDBY" } },
            { TagType::EncodingTime, { "ENCODINGTIME", "TDEN" } },
            { TagType::Engineer, { "ENGINEER" } },
            { TagType::GaplessPlayback, { "GAPLESSPLAYBACK" } },
            { TagType::Genre, { "GENRE" } },
            { TagType::Grouping, { "GROUPING", "WM/CONTENTGROUPDESCRIPTION", "ALBUMGROUPING" } },
            { TagType::InitialKey, { "INITIALKEY" } },
            { TagType::ISRC, { "ISRC", "WM/ISRC", "TSRC" } },
            { TagType::Language, { "LANGUAGE" } },
            { TagType::License, { "LICENSE" } },
            { TagType::Lyricist, { "LYRICIST" } },
            { TagType::LyricistSortOrder, { "LYRICISTSORT" } },
            { TagType::Lyricists, { "LYRICISTS" } },
            { TagType::LyricistsSortOrder, { "LYRICISTSSORT" } },
            { TagType::Media, { "TMED", "MEDIA", "WM/MEDIA" } },
            { TagType::MixDJ, { "DJMIXER" } },
            { TagType::Mixer, { "MIXER" } },
            { TagType::MixerSortOrder, { "MIXERSORT" } },
            { TagType::Mixers, { "MIXERS" } },
            { TagType::MixersSortOrder, { "MIXERSSORT" } },
            { TagType::Mood, { "MOOD" } },
            { TagType::Movement, { "MOVEMENT", "MOVEMENTNAME" } },
            { TagType::MovementCount, { "MOVEMENTCOUNT" } },
            { TagType::MovementNumber, { "MOVEMENTNUMBER" } },
            { TagType::MusicBrainzArtistID, { "MUSICBRAINZ_ARTISTID", "MUSICBRAINZ ARTIST ID", "MUSICBRAINZ/ARTIST ID" } },
            { TagType::MusicBrainzArrangerID, { "MUSICBRAINZ_ARRANGERID", "MUSICBRAINZ ARRANGER ID", "MUSICBRAINZ/ARRANGER ID" } },
            { TagType::MusicBrainzComposerID, { "MUSICBRAINZ_COMPOSERID", "MUSICBRAINZ COMPOSER ID", "MUSICBRAINZ/COMPOSER ID" } },
            { TagType::MusicBrainzConductorID, { "MUSICBRAINZ_CONDUCTORID", "MUSICBRAINZ CONDUCTOR ID", "MUSICBRAINZ/CONDUCTOR ID" } },
            { TagType::MusicBrainzDirectorID, { "MUSICBRAINZ_DIRECTORID", "MUSICBRAINZ DIRECTOR ID", "MUSICBRAINZ/DIRECTOR ID" } },
            { TagType::MusicBrainzDiscID, { "MUSICBRAINZ_DISCID", "MUSICBRAINZ DISC ID", "MUSICBRAINZ/DISC ID" } },
            { TagType::MusicBrainzLyricistID, { "MUSICBRAINZ_LYRICISTID", "MUSICBRAINZ LYRICIST ID", "MUSICBRAINZ/LYRICIST ID" } },
            { TagType::MusicBrainzOriginalArtistID, { "MUSICBRAINZ_ORIGINALARTISTID", "MUSICBRAINZ ORIGINAL ARTIST ID", "MUSICBRAINZ/ORIGINAL ARTIST ID" } },
            { TagType::MusicBrainzOriginalReleaseID, { "MUSICBRAINZ_ORIGINALRELEASEID", "MUSICBRAINZ ORIGINAL RELEASE ID", "MUSICBRAINZ/ORIGINAL RELEASE ID" } },
            { TagType::MusicBrainzMixerID, { "MUSICBRAINZ_MIXERID", "MUSICBRAINZ MIXER ID", "MUSICBRAINZ/MIXER ID" } },
            { TagType::MusicBrainzProducerID, { "MUSICBRAINZ_PRODUCERID", "MUSICBRAINZ PRODUCER ID", "MUSICBRAINZ/PRODUCER ID" } },
            { TagType::MusicBrainzRecordingID, { "MUSICBRAINZ_TRACKID", "MUSICBRAINZ TRACK ID", "MUSICBRAINZ/TRACK ID" } },
            { TagType::MusicBrainzReleaseArtistID, { "MUSICBRAINZ_ALBUMARTISTID", "MUSICBRAINZ ALBUM ARTIST ID", "MUSICBRAINZ/ALBUM ARTIST ID" } },
            { TagType::MusicBrainzReleaseGroupID, { "MUSICBRAINZ_RELEASEGROUPID", "MUSICBRAINZ RELEASE GROUP ID", "MUSICBRAINZ/RELEASE GROUP ID" } },
            { TagType::MusicBrainzReleaseID, { "MUSICBRAINZ_ALBUMID", "MUSICBRAINZ ALBUM ID", "MUSICBRAINZ/ALBUM ID" } },
            { TagType::MusicBrainzRemixerID, { "MUSICBRAINZ_REMIXERID", "MUSICBRAINZ REMIXER ID", "MUSICBRAINZ/REMIXER ID" } },
            { TagType::MusicBrainzTrackID, { "MUSICBRAINZ_RELEASETRACKID", "MUSICBRAINZ RELEASE TRACK ID", "MUSICBRAINZ/RELEASE TRACK ID" } },
            { TagType::MusicBrainzWorkID, { "MUSICBRAINZ_WORKID", "MUSICBRAINZ WORK ID", "MUSICBRAINZ/WORK ID" } },
            { TagType::OriginalArtist, { "ORIGINALARTIST" } },
            { TagType::OriginalFilename, { "ORIGINALFILENAME" } },
            { TagType::OriginalReleaseDate, { "ORIGINALDATE", "TDOR", "WM/ORIGINALRELEASETIME" } },
            { TagType::OriginalReleaseYear, { "ORIGINALYEAR", "TORY", "WM/ORIGINALRELEASEYEAR" } },
            { TagType::Podcast, { "PODCAST" } },
            { TagType::PodcastURL, { "PODCASTURL" } },
            { TagType::Producer, { "PRODUCER" } },
            { TagType::ProducerSortOrder, { "PRODUCERSORTORDER" } },
            { TagType::Producers, { "PRODUCERS" } },
            { TagType::ProducersSortOrder, { "PRODUCERSSORTORDER" } },
            { TagType::RecordLabel, { "LABEL", "PUBLISHER", "ORGANIZATION" } },
            { TagType::ReleaseCountry, { "RELEASECOUNTRY" } },
            { TagType::ReleaseDate, { "RELEASEDATE" } },
            { TagType::ReleaseStatus, { "RELEASESTATUS" } },
            { TagType::ReleaseType, { "RELEASETYPE", "MUSICBRAINZ_ALBUMTYPE", "MUSICBRAINZ ALBUM TYPE", "MUSICBRAINZ/ALBUM TYPE" } },
            { TagType::Remixer, { "REMIXER", "MODIFIEDBY", "MIXARTIST" } },
            { TagType::RemixerSortOrder, { "REMIXERSORTORDER", "MIXARTISTSORTORDER" } },
            { TagType::Remixers, { "REMIXERS" } },
            { TagType::RemixersSortOrder, { "REMIXERSSORTORDER", "MIXARTISTSSORTORDER" } },
            { TagType::ReplayGainAlbumGain, { "REPLAYGAIN_ALBUM_GAIN" } },
            { TagType::ReplayGainAlbumPeak, { "REPLAYGAIN_ALBUM_PEAK" } },
            { TagType::ReplayGainAlbumRange, { "REPLAYGAIN_ALBUM_RANGE" } },
            { TagType::ReplayGainReferenceLoudness, { "REPLAYGAIN_REFERENCE_LOUDNESS" } },
            { TagType::ReplayGainTrackGain, { "REPLAYGAIN_TRACK_GAIN" } },
            { TagType::ReplayGainTrackPeak, { "REPLAYGAIN_TRACK_PEAK" } },
            { TagType::ReplayGainTrackRange, { "REPLAYGAIN_TRACK_RANGE" } },
            { TagType::Script, { "SCRIPT", "WM/SCRIPT" } },
            { TagType::ShowWorkAndMovement, { "SHOWWORKMOVEMENT", "SHOWMOVEMENT" } },
            { TagType::Subtitle, { "SUBTITLE" } },
            { TagType::TotalDiscs, { "DISCTOTAL", "TOTALDISCS" } },
            { TagType::TotalTracks, { "TRACKTOTAL", "TOTALTRACKS" } },
            { TagType::TrackNumber, { "TRCK", "TRACK", "TRACKNUMBER", "TRKN", "WM/TRACKNUMBER" } },
            { TagType::TrackTitle, { "TITLE" } },
            { TagType::TrackTitleSortOrder, { "TITLESORT" } },
            { TagType::WorkTitle, { "WORK" } },
            { TagType::Writer, { "WRITER" } },
        };
    } // namespace

    AvFormatTagReader::AvFormatTagReader(const std::filesystem::path& p, bool debug)
    {
        try
        {
            const auto audioFile{ av::parseAudioFile(p) };

            _audioProperties.duration = audioFile->getContainerInfo().duration;

            const auto bestAudioStream{ audioFile->getBestStreamInfo() };
            if (bestAudioStream)
            {
                _audioProperties.bitrate = bestAudioStream->bitrate;
                _audioProperties.bitsPerSample = bestAudioStream->bitsPerSample;
                _audioProperties.channelCount = bestAudioStream->channelCount;
                _audioProperties.sampleRate = bestAudioStream->sampleRate;
            }

            _containerInfo = audioFile->getContainerInfo();
            _metaDataMap = audioFile->getMetaData();

            if (debug && core::Service<core::logging::ILogger>::get()->isSeverityActive(core::logging::Severity::DEBUG))
            {
                for (const auto& [key, value] : _metaDataMap)
                    LMS_LOG(METADATA, DEBUG, "Key = '" << key << "', value = '" << value << "'");
            }
        }
        catch (av::Exception& e)
        {
            throw AudioFileParsingException{ e.what() };
        }
    }

    AvFormatTagReader::~AvFormatTagReader() = default;

    void AvFormatTagReader::visitTagValues(TagType tag, TagValueVisitor visitor) const
    {
        auto itTagNames{ avFormatTagMapping.find(tag) };
        if (itTagNames == std::cend(avFormatTagMapping))
            return;

        for (const std::string& tagName : itTagNames->second)
        {
            bool visited{};

            visitTagValues(tagName, [&](std::string_view value) {
                visited = true;
                visitor(value);
            });

            if (visited)
                break;
        }
    }

    void AvFormatTagReader::visitTagValues(std::string_view key, TagValueVisitor visitor) const
    {
        auto itValues{ _metaDataMap.find(std::string{ key }) };
        if (itValues == std::cend(_metaDataMap))
            return;

        visitor(itValues->second);
    }

    void AvFormatTagReader::visitPerformerTags(PerformerVisitor visitor) const
    {
        visitTagValues("PERFORMER", [&](std::string_view value) {
            visitor("", value);
        });
    }

    void AvFormatTagReader::visitLyricsTags(LyricsVisitor visitor) const
    {
        // MPEG files: need to visit LYRICS-language entries
        for (const auto& [tag, value] : _metaDataMap)
        {
            constexpr std::string_view lyricsPrefix{ "LYRICS-" };
            if (tag.starts_with(lyricsPrefix))
            {
                const std::string language{ core::stringUtils::stringToLower(tag.substr(lyricsPrefix.size())) };
                visitor(language, value);
            }
        }

        // otherwise, just visit regular LYRICS tag with no language
        visitTagValues("LYRICS", [&](std::string_view value) {
            visitor("", value);
        });
    }
} // namespace lms::metadata::avformat
