/*
 * Copyright (C) 2016 Emeric Poupon
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

#include "TagLibTagReader.hpp"

#include <unordered_map>

#include <taglib/aifffile.h>
#include <taglib/apeproperties.h>
#include <taglib/apetag.h>
#include <taglib/asffile.h>
#if TAGLIB_MAJOR_VERSION >= 2
    #include <taglib/dsffile.h>
#endif
#include <taglib/fileref.h>
#include <taglib/flacfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/mp4file.h>
#include <taglib/mpcfile.h>
#include <taglib/mpegfile.h>
#include <taglib/opusfile.h>
#include <taglib/synchronizedlyricsframe.h>
#include <taglib/tag.h>
#include <taglib/tpropertymap.h>
#include <taglib/unsynchronizedlyricsframe.h>
#include <taglib/vorbisfile.h>
#include <taglib/wavfile.h>
#include <taglib/wavpackfile.h>

#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"
#include "core/String.hpp"
#include "metadata/Exception.hpp"

namespace lms::metadata
{
    namespace
    {
        class ParsingFailedException : public Exception
        {
        };

        // Mapping to internal taglib names and/or common alternative custom names
        static const std::unordered_map<TagType, std::vector<std::string>> tagMapping{
            { TagType::AcoustID, { "ACOUSTID_ID", "ACOUSTID ID" } },
            { TagType::Album, { "ALBUM" } },
            { TagType::AlbumArtist, { "ALBUMARTIST" } },
            { TagType::AlbumArtistSortOrder, { "ALBUMARTISTSORT" } },
            { TagType::AlbumArtists, { "ALBUMARTISTS" } },
            { TagType::AlbumArtistsSortOrder, { "ALBUMARTISTSSORT" } },
            { TagType::AlbumSortOrder, { "ALBUMSORT" } },
            { TagType::Arranger, { "ARRANGER" } },
            { TagType::Artist, { "ARTIST" } },
            { TagType::ArtistSortOrder, { "ARTISTSORT" } },
            { TagType::Artists, { "ARTISTS" } },
            { TagType::ASIN, { "ASIN" } },
            { TagType::Barcode, { "BARCODE" } },
            { TagType::BPM, { "BPM" } },
            { TagType::CatalogNumber, { "CATALOGNUMBER" } },
            { TagType::Comment, { "COMMENT" } },
            { TagType::Compilation, { "COMPILATION" } },
            { TagType::Composer, { "COMPOSER" } },
            { TagType::Composers, { "COMPOSERS" } },
            { TagType::ComposerSortOrder, { "COMPOSERSORT" } },
            { TagType::ComposersSortOrder, { "COMPOSERSSORT" } },
            { TagType::Conductor, { "CONDUCTOR" } },
            { TagType::ConductorSortOrder, { "CONDUCTORSORT" } },
            { TagType::Conductors, { "CONDUCTORS" } },
            { TagType::ConductorsSortOrder, { "CONDUCTORSSORT" } },
            { TagType::Copyright, { "COPYRIGHT" } },
            { TagType::CopyrightURL, { "COPYRIGHTURL" } },
            { TagType::Date, { "DATE", "YEAR" } },
            { TagType::Director, { "DIRECTOR" } },
            { TagType::DiscNumber, { "DISCNUMBER", "DISC" } },
            { TagType::DiscSubtitle, { "DISCSUBTITLE", "SETSUBTITLE" } },
            { TagType::EncodedBy, { "ENCODEDBY" } },
            { TagType::Engineer, { "ENGINEER" } },
            { TagType::GaplessPlayback, { "GAPLESSPLAYBACK" } },
            { TagType::Genre, { "GENRE" } },
            { TagType::Grouping, { "GROUPING", "ALBUMGROUPING" } },
            { TagType::InitialKey, { "INITIALKEY" } },
            { TagType::ISRC, { "ISRC" } },
            { TagType::Language, { "LANGUAGE" } },
            { TagType::License, { "LICENSE" } },
            { TagType::Lyricist, { "LYRICIST" } },
            { TagType::LyricistSortOrder, { "LYRICISTSORT" } },
            { TagType::Lyricists, { "LYRICISTS" } },
            { TagType::LyricistsSortOrder, { "LYRICISTSSORT" } },
            { TagType::Media, { "MEDIA" } },
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
            { TagType::MusicBrainzDiscID, { "MUSICBRAINZ_DISCID", "MUSICBRAINZ DISC ID", "MUSICBRAINZ/DISC ID" } },
            { TagType::MusicBrainzOriginalArtistID, { "MUSICBRAINZ_ORIGINALARTISTID", "MUSICBRAINZ ORIGINAL ARTIST ID", "MUSICBRAINZ/ORIGINAL ARTIST ID" } },
            { TagType::MusicBrainzOriginalReleaseID, { "MUSICBRAINZ_ORIGINALRELEASEID", "MUSICBRAINZ ORIGINAL RELEASE ID", "MUSICBRAINZ/ORIGINAL RELEASE ID" } },
            { TagType::MusicBrainzRecordingID, { "MUSICBRAINZ_TRACKID", "MUSICBRAINZ TRACK ID", "MUSICBRAINZ/TRACK ID" } },
            { TagType::MusicBrainzReleaseArtistID, { "MUSICBRAINZ_ALBUMARTISTID", "MUSICBRAINZ ALBUM ARTIST ID", "MUSICBRAINZ/ALBUM ARTIST ID" } },
            { TagType::MusicBrainzReleaseGroupID, { "MUSICBRAINZ_RELEASEGROUPID", "MUSICBRAINZ RELEASE GROUP ID", "MUSICBRAINZ/RELEASE GROUP ID" } },
            { TagType::MusicBrainzReleaseID, { "MUSICBRAINZ_ALBUMID", "MUSICBRAINZ ALBUM ID", "MUSICBRAINZ/ALBUM ID" } },
            { TagType::MusicBrainzTrackID, { "MUSICBRAINZ_RELEASETRACKID", "MUSICBRAINZ RELEASE TRACK ID", "MUSICBRAINZ/RELEASE TRACK ID" } },
            { TagType::MusicBrainzWorkID, { "MUSICBRAINZ_WORKID", "MUSICBRAINZ WORK ID", "MUSICBRAINZ/WORK ID" } },
            { TagType::OriginalArtist, { "ORIGINALARTIST" } },
            { TagType::OriginalFilename, { "ORIGINALFILENAME" } },
            { TagType::OriginalReleaseDate, { "ORIGINALDATE" } },
            { TagType::OriginalReleaseYear, { "ORIGINALYEAR" } },
            { TagType::Podcast, { "PODCAST" } },
            { TagType::PodcastURL, { "PODCASTURL" } },
            { TagType::Producer, { "PRODUCER" } },
            { TagType::ProducerSortOrder, { "PRODUCERSORTORDER" } },
            { TagType::Producers, { "PRODUCERS" } },
            { TagType::ProducersSortOrder, { "PRODUCERSSORTORDER" } },
            { TagType::RecordLabel, { "LABEL" } },
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
            { TagType::Script, { "SCRIPT" } },
            { TagType::ShowWorkAndMovement, { "SHOWWORKMOVEMENT", "SHOWMOVEMENT" } },
            { TagType::Subtitle, { "SUBTITLE" } },
            { TagType::TotalDiscs, { "DISCTOTAL", "TOTALDISCS" } },
            { TagType::TotalTracks, { "TRACKTOTAL", "TOTALTRACKS" } },
            { TagType::TrackNumber, { "TRACKNUMBER" } },
            { TagType::TrackTitle, { "TITLE" } },
            { TagType::TrackTitleSortOrder, { "TITLESORT" } },
            { TagType::WorkTitle, { "WORK" } },
            { TagType::Writer, { "WRITER" } },
        };

        TagLib::AudioProperties::ReadStyle readStyleToTagLibReadStyle(ParserReadStyle readStyle)
        {
            switch (readStyle)
            {
            case ParserReadStyle::Fast:
                return TagLib::AudioProperties::ReadStyle::Fast;
            case ParserReadStyle::Average:
                return TagLib::AudioProperties::ReadStyle::Average;
            case ParserReadStyle::Accurate:
                return TagLib::AudioProperties::ReadStyle::Accurate;
            }

            throw core::LmsException{ "Cannot convert read style" };
        }

        void mergeTagMaps(TagLib::PropertyMap& dst, TagLib::PropertyMap&& src)
        {
            for (auto&& [tag, values] : src)
            {
                if (dst.find(tag) == std::cend(dst))
                    dst[tag] = std::move(values);
            }
        }

        TagLib::FileRef parseFile(const std::filesystem::path& p, ParserReadStyle parserReadStyle)
        {
            LMS_SCOPED_TRACE_DETAILED("MetaData", "TagLibParseFile");

            return TagLib::FileRef{ p.string().c_str(), true // read audio properties
                ,
                readStyleToTagLibReadStyle(parserReadStyle) };
        }
    } // namespace

    TagLibTagReader::TagLibTagReader(const std::filesystem::path& p, ParserReadStyle parserReadStyle, bool debug)
        : _file{ parseFile(p, parserReadStyle) }
    {
        if (_file.isNull())
        {
            LMS_LOG(METADATA, ERROR, "File '" << p.string() << "': parsing failed");
            throw ParsingFailedException{};
        }

        if (!_file.audioProperties())
        {
            LMS_LOG(METADATA, ERROR, "File '" << p.string() << "': no audio properties");
            throw ParsingFailedException{};
        }

        computeAudioProperties();

        _propertyMap = _file.file()->properties();

        // Some tags may not be known by TagLib
        auto getAPETags = [&](const TagLib::APE::Tag* apeTag) {
            if (!apeTag)
                return;

            mergeTagMaps(_propertyMap, apeTag->properties());
        };

        auto processID3v2Tags = [&](TagLib::ID3v2::Tag& id3v2Tags) {
            const auto& frameListMap{ id3v2Tags.frameListMap() };

            // Not that good embedded pictures handling
            if (!frameListMap["APIC"].isEmpty())
                _hasEmbeddedCover = true;

            // Get some extra tags that may not be known by taglib
            if (!frameListMap["TSST"].isEmpty() && !_propertyMap.contains("DISCSUBTITLE"))
                _propertyMap["DISCSUBTITLE"] = { frameListMap["TSST"].front()->toString() };

            // consider each frame hold a different set of lyrics
            // Synchronized lyrics frames
            for (const TagLib::ID3v2::Frame* frame : frameListMap["SYLT"])
            {
                const auto* lyricsFrame{ dynamic_cast<const TagLib::ID3v2::SynchronizedLyricsFrame*>(frame) };
                if (!lyricsFrame)
                    continue; // TODO log or assert?

                const std::string language{ lyricsFrame->language().data(), lyricsFrame->language().size() };
                std::string lyrics;
                for (const TagLib::ID3v2::SynchronizedLyricsFrame::SynchedText& synchedText : lyricsFrame->synchedText())
                {
                    std::chrono::milliseconds timestamp{};
                    switch (lyricsFrame->timestampFormat())
                    {
                    case TagLib::ID3v2::SynchronizedLyricsFrame::AbsoluteMilliseconds:
                        timestamp = std::chrono::milliseconds{ synchedText.time };
                        break;
                    case TagLib::ID3v2::SynchronizedLyricsFrame::AbsoluteMpegFrames:
                        timestamp = std::chrono::milliseconds{ _audioProperties.sampleRate ? (synchedText.time * 1000) / _audioProperties.sampleRate : 0 };
                        break;
                    case TagLib::ID3v2::SynchronizedLyricsFrame::Unknown:
                        break;
                    }

                    if (!lyrics.empty())
                        lyrics += '\n';

                    lyrics += core::stringUtils::formatTimestamp(timestamp);
                    lyrics += synchedText.text.to8Bit(true);
                }

                _id3v2Lyrics.emplace(language, std::move(lyrics));
            }

            // Unsynchronized lyrics frames
            for (const TagLib::ID3v2::Frame* frame : frameListMap["USLT"])
            {
                const auto* lyricsFrame{ dynamic_cast<const TagLib::ID3v2::UnsynchronizedLyricsFrame*>(frame) };
                if (!lyricsFrame)
                    continue; // TODO log or assert?

                const std::string language{ lyricsFrame->language().data(), lyricsFrame->language().size() };
                _id3v2Lyrics.emplace(language, lyricsFrame->text().to8Bit(true));
            }
        };

        // WMA
        if (TagLib::ASF::File * asfFile{ dynamic_cast<TagLib::ASF::File*>(_file.file()) })
        {
            if (const TagLib::ASF::Tag * tag{ asfFile->tag() })
            {
                if (tag->attributeListMap().contains("WM/Picture"))
                    _hasEmbeddedCover = true;

                for (const auto& [name, attributeList] : tag->attributeListMap())
                {
                    if (attributeList.isEmpty())
                        continue;

                    std::string strName{ core::stringUtils::stringToUpper(name.to8Bit(true)) };
                    if (strName.find("WM/") == 0 || _propertyMap.find(strName) != std::cend(_propertyMap))
                        continue;

                    TagLib::StringList attributes;
                    for (const TagLib::ASF::Attribute& attribute : attributeList)
                    {
                        if (attribute.type() == TagLib::ASF::Attribute::AttributeTypes::UnicodeType)
                            attributes.append(attribute.toString());
                    }

                    if (!attributes.isEmpty())
                        _propertyMap[strName] = std::move(attributes);
                }
            }
        }
        // MP3
        else if (TagLib::MPEG::File * mp3File{ dynamic_cast<TagLib::MPEG::File*>(_file.file()) })
        {
            if (mp3File->hasID3v2Tag())
                processID3v2Tags(*mp3File->ID3v2Tag());

            getAPETags(mp3File->APETag());
        }
        // MP4
        else if (TagLib::MP4::File * mp4File{ dynamic_cast<TagLib::MP4::File*>(_file.file()) })
        {
            TagLib::MP4::Item coverItem{ mp4File->tag()->item("covr") };
            TagLib::MP4::CoverArtList coverArtList{ coverItem.toCoverArtList() };
            if (!coverArtList.isEmpty())
                _hasEmbeddedCover = true;

            if (!_propertyMap.contains("ORIGINALDATE"))
            {
                // For now:
                // * TagLib 2.0 only parses ----:com.apple.iTunes:ORIGINALDATE
                // / TagLib <2.0 only parses ----:com.apple.iTunes:originaldate
                const auto& tags{ mp4File->tag()->itemMap() };
                for (const auto& origDateString : { "----:com.apple.iTunes:originaldate", "----:com.apple.iTunes:ORIGINALDATE" })
                {
                    auto itOrigDateTag{ tags.find(origDateString) };
                    if (itOrigDateTag != std::cend(tags))
                    {
                        const TagLib::StringList dates{ itOrigDateTag->second.toStringList() };
                        if (!dates.isEmpty())
                        {
                            _propertyMap["ORIGINALDATE"] = dates.front();
                            break;
                        }
                    }
                }
            }
        }
        // MPC
        else if (TagLib::MPC::File * mpcFile{ dynamic_cast<TagLib::MPC::File*>(_file.file()) })
        {
            getAPETags(mpcFile->APETag());
        }
        // WavPack
        else if (TagLib::WavPack::File * wavPackFile{ dynamic_cast<TagLib::WavPack::File*>(_file.file()) })
        {
            getAPETags(wavPackFile->APETag());
        }
        // FLAC
        else if (TagLib::FLAC::File * flacFile{ dynamic_cast<TagLib::FLAC::File*>(_file.file()) })
        {
            if (!flacFile->pictureList().isEmpty())
                _hasEmbeddedCover = true;
        }
        else if (TagLib::Ogg::Vorbis::File * vorbisFile{ dynamic_cast<TagLib::Ogg::Vorbis::File*>(_file.file()) })
        {
            if (!vorbisFile->tag()->pictureList().isEmpty())
                _hasEmbeddedCover = true;
        }
        else if (TagLib::Ogg::Opus::File * opusFile{ dynamic_cast<TagLib::Ogg::Opus::File*>(_file.file()) })
        {
            if (!opusFile->tag()->pictureList().isEmpty())
                _hasEmbeddedCover = true;
        }
        else if (TagLib::RIFF::AIFF::File * aiffFile{ dynamic_cast<TagLib::RIFF::AIFF::File*>(_file.file()) })
        {
            if (aiffFile->hasID3v2Tag())
                processID3v2Tags(*aiffFile->tag());
        }
        else if (TagLib::RIFF::WAV::File * wavFile{ dynamic_cast<TagLib::RIFF::WAV::File*>(_file.file()) })
        {
            if (wavFile->hasID3v2Tag())
                processID3v2Tags(*wavFile->ID3v2Tag());
        }

        if (debug && core::Service<core::logging::ILogger>::get()->isSeverityActive(core::logging::Severity::DEBUG))
        {
            for (const auto& [key, values] : _propertyMap)
            {
                for (const auto& value : values)
                    LMS_LOG(METADATA, DEBUG, "Key = '" << key << "', value = '" << value.to8Bit(true) << "'");
            }

            for (const auto& value : _propertyMap.unsupportedData())
                LMS_LOG(METADATA, DEBUG, "Unknown value: '" << value.to8Bit(true) << "'");
        }
    }

    void TagLibTagReader::computeAudioProperties()
    {
        const TagLib::AudioProperties* properties{ _file.audioProperties() };

        // Common properties
        _audioProperties.bitrate = static_cast<std::size_t>(properties->bitrate() * 1000);
        _audioProperties.channelCount = static_cast<std::size_t>(_file.audioProperties()->channels());
        _audioProperties.duration = std::chrono::milliseconds{ properties->lengthInMilliseconds() };
        _audioProperties.sampleRate = static_cast<std::size_t>(properties->sampleRate());

        if (const auto* apeProperties{ dynamic_cast<const TagLib::APE::Properties*>(properties) })
            _audioProperties.bitsPerSample = apeProperties->bitsPerSample();
        if (const auto* asfProperties{ dynamic_cast<const TagLib::ASF::Properties*>(properties) })
            _audioProperties.bitsPerSample = asfProperties->bitsPerSample();
        else if (const auto* flacProperties{ dynamic_cast<const TagLib::FLAC::Properties*>(properties) })
            _audioProperties.bitsPerSample = flacProperties->bitsPerSample();
        else if (const auto* mp4Properties{ dynamic_cast<const TagLib::MP4::Properties*>(properties) })
            _audioProperties.bitsPerSample = mp4Properties->bitsPerSample();
        else if (const auto* wavePackProperties{ dynamic_cast<const TagLib::WavPack::Properties*>(properties) })
            _audioProperties.bitsPerSample = wavePackProperties->bitsPerSample();
        else if (const auto* aiffProperties{ dynamic_cast<const TagLib::RIFF::AIFF::Properties*>(properties) })
            _audioProperties.bitsPerSample = aiffProperties->bitsPerSample();
        else if (const auto* wavProperties{ dynamic_cast<const TagLib::RIFF::WAV::Properties*>(properties) })
            _audioProperties.bitsPerSample = wavProperties->bitsPerSample();
#if TAGLIB_MAJOR_VERSION >= 2
        else if (const auto* dsfProperties{ dynamic_cast<const TagLib::DSF::Properties*>(properties) })
            _audioProperties.bitsPerSample = dsfProperties->bitsPerSample();
#endif
    }

    void TagLibTagReader::visitTagValues(TagType tag, TagValueVisitor visitor) const
    {
        auto itTagNames{ tagMapping.find(tag) };
        if (itTagNames == std::cend(tagMapping))
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

    void TagLibTagReader::visitTagValues(std::string_view tag, TagValueVisitor visitor) const
    {
        TagLib::String key{ tag.data() /* assume null terminated */, TagLib::String::Type::UTF8 };

        auto itValues{ _propertyMap.find(key) };
        if (itValues == std::cend(_propertyMap))
            return;

        for (const TagLib::String& value : itValues->second)
            visitor(value.to8Bit(true));
    }

    void TagLibTagReader::visitPerformerTags(PerformerVisitor visitor) const
    {
        visitTagValues("PERFORMER", [&](std::string_view value) {
            visitor("", value);
        });

        for (const auto& [key, values] : _propertyMap)
        {
            if (key.startsWith("PERFORMER:")) // startsWith is not case sensitive
            {
                std::string performerStr{ key.to8Bit(true) };
                const std::size_t rolePos{ performerStr.find(':') };
                assert(rolePos != std::string::npos);

                std::string_view role{ std::string_view{ performerStr }.substr(rolePos + 1) };
                for (const TagLib::String& value : values)
                {
                    const std::string name{ value.to8Bit(true) };
                    visitor(role, name);
                }
            }
        }
    }

    void TagLibTagReader::visitLyricsTags(LyricsVisitor visitor) const
    {
        if (!_id3v2Lyrics.empty())
        {
            for (const auto& [language, lyrics] : _id3v2Lyrics)
                visitor(language, lyrics);
        }
        else
        {
            // otherwise, just visit regular LYRICS tag with no language
            visitTagValues("LYRICS", [&](std::string_view value) {
                visitor("", value);
            });
        }
    }
} // namespace lms::metadata
