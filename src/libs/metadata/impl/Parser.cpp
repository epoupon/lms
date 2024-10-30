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

#include "Parser.hpp"

#include <span>

#include "core/ILogger.hpp"
#include "core/String.hpp"
#include "metadata/Exception.hpp"

#include "AvFormatTagReader.hpp"
#include "TagLibTagReader.hpp"
#include "Utils.hpp"

namespace lms::metadata
{
    namespace
    {
        void visitTagValues(const ITagReader& tagReader, std::string_view tagType, std::span<const std::string> tagDelimiters, ITagReader::TagValueVisitor visitor)
        {
            tagReader.visitTagValues(tagType, [&](std::string_view value) {
                auto visitTagIfNonEmpty{ [&](std::string_view tag) {
                    tag = core::stringUtils::stringTrim(tag);
                    if (!tag.empty())
                        visitor(tag);
                } };

                for (std::string_view tagDelimiter : tagDelimiters)
                {
                    if (value.find(tagDelimiter) != std::string_view::npos)
                    {
                        for (std::string_view splitTag : core::stringUtils::splitString(value, tagDelimiters))
                            visitTagIfNonEmpty(splitTag);

                        return;
                    }
                }

                // no delimiter found, or no delimiter to be used
                visitTagIfNonEmpty(value);
            });
        }

        template<typename T>
        std::vector<T> getTagValuesFirstMatchAs(const ITagReader& tagReader, std::initializer_list<TagType> tagTypes, std::span<const std::string> tagDelimiters)
        {
            std::vector<T> res;

            for (const TagType tagType : tagTypes)
            {
                auto addTagIfNonEmpty{ [&res](std::string_view tag) {
                    tag = core::stringUtils::stringTrim(tag);
                    if (!tag.empty())
                    {
                        std::optional<T> val{ core::stringUtils::readAs<T>(tag) };
                        if (val)
                            res.emplace_back(std::move(*val));
                    }
                } };

                tagReader.visitTagValues(tagType, [&](std::string_view value) {
                    for (std::string_view tagDelimiter : tagDelimiters)
                    {
                        if (value.find(tagDelimiter) != std::string_view::npos)
                        {
                            for (std::string_view splitTag : core::stringUtils::splitString(value, tagDelimiters))
                                addTagIfNonEmpty(splitTag);

                            return;
                        }
                    }

                    // no delimiter found, or no delimiter to be used
                    addTagIfNonEmpty(value);
                });

                if (!res.empty())
                    break;
            }

            return res;
        }

        template<typename T>
        std::optional<T> getTagValueFirstMatchAs(const ITagReader& tagReader, std::initializer_list<TagType> tagTypes)
        {
            std::optional<T> res;
            std::vector<T> values{ getTagValuesFirstMatchAs<T>(tagReader, tagTypes, {} /* don't expect multiple values here */) };
            if (!values.empty())
                res = std::move(values.front());

            return res;
        }

        template<typename T>
        std::vector<T> getTagValuesAs(const ITagReader& tagReader, TagType tagType, std::span<const std::string> tagDelimiters)
        {
            return getTagValuesFirstMatchAs<T>(tagReader, { tagType }, tagDelimiters);
        }

        template<typename T>
        std::optional<T> getTagValueAs(const ITagReader& tagReader, TagType tagType)
        {
            return getTagValueFirstMatchAs<T>(tagReader, { tagType });
        }

        std::vector<Lyrics> getLyrics(const ITagReader& tagReader)
        {
            std::vector<Lyrics> res;

            tagReader.visitLyricsTags([&](std::string_view language, std::string_view lyricsText) {
                std::istringstream iss{ std::string{ lyricsText } }; // TODO avoid copies (ispanstream?)
                try
                {
                    Lyrics lyrics{ parseLyrics(iss) };
                    if (lyrics.language.empty())
                        lyrics.language = language;

                    res.emplace_back(std::move(lyrics));
                }
                catch (const LyricsException& e)
                {
                    LMS_LOG(METADATA, ERROR, "Failed to parse lyrics: " + std::string{ e.what() });
                }
            });

            return res;
        }

        std::vector<Artist> getArtists(const ITagReader& tagReader,
            std::initializer_list<TagType> artistTagNames,
            std::initializer_list<TagType> artistSortTagNames,
            std::initializer_list<TagType> artistMBIDTagNames,
            std::span<const std::string> artistTagDelimiters,
            std::span<const std::string> defaultTagDelimiters)
        {
            std::vector<std::string> artistNames{ getTagValuesFirstMatchAs<std::string>(tagReader, artistTagNames, artistTagDelimiters) };
            if (artistNames.empty())
                return {};

            std::vector<std::string> artistSortNames{ getTagValuesFirstMatchAs<std::string>(tagReader, artistSortTagNames, artistTagDelimiters) };
            std::vector<core::UUID> artistMBIDs{ getTagValuesFirstMatchAs<core::UUID>(tagReader, artistMBIDTagNames, defaultTagDelimiters) };

            std::vector<Artist> artists;
            artists.reserve(artistNames.size());

            for (std::size_t i{}; i < artistNames.size(); ++i)
            {
                Artist& artist{ artists.emplace_back(std::move(artistNames[i])) };

                if (artistNames.size() == artistSortNames.size())
                    artist.sortName = std::move(artistSortNames[i]);
                if (artistNames.size() == artistMBIDs.size())
                    artist.mbid = std::move(artistMBIDs[i]);
            }

            return artists;
        }

        PerformerContainer getPerformerArtists(const ITagReader& tagReader)
        {
            PerformerContainer performers;

            tagReader.visitPerformerTags([&](std::string_view role, std::string_view name) {
                // picard stores like this: (see https://picard-docs.musicbrainz.org/en/appendices/tag_mapping.html#performer)
                // We consider we may hit both styles for the same track
                if (role.empty())
                {
                    // "PERFORMER" "artist (role)"
                    utils::PerformerArtist performer{ utils::extractPerformerAndRole(name) };
                    core::stringUtils::capitalize(performer.role);
                    performers[performer.role].push_back(std::move(performer.artist));
                }
                else
                {
                    // "PERFORMER:role", "artist" (MP3)
                    std::string roleCapitalized{ core::stringUtils::stringToLower(role) };
                    core::stringUtils::capitalize(roleCapitalized);
                    performers[roleCapitalized].push_back(Artist{ name });
                }
            });

            return performers;
        }

        bool strIsMatchingArtistNames(std::string_view str, std::span<const std::string_view> artistNames)
        {
            std::string_view::size_type currentOffset{};

            for (const std::string_view artistName : artistNames)
            {
                std::string_view::size_type newPos{ str.find(artistName, currentOffset) };
                if (newPos == std::string_view::npos)
                    return false;

                currentOffset = newPos + artistName.size();
            }

            return true;
        }

        bool strIsContainingAny(std::string_view str, std::span<const std::string> subStrs)
        {
            return std::any_of(std::cbegin(subStrs), std::cend(subStrs), [&str](const std::string& subStr) { return str.find(subStr) != std::string_view::npos; });
        }

        std::string computeArtistDisplayName(std::span<const Artist> artists, const std::optional<std::string> artistTag, std::span<const std::string> artistTagDelimiters)
        {
            std::string artistDisplayName;

            if (artists.size() == 1)
                artistDisplayName = artists.front().name;
            else if (artists.size() > 1)
            {
                std::vector<std::string_view> artistNames;
                std::transform(std::cbegin(artists), std::cend(artists), std::back_inserter(artistNames), [](const Artist& artist) -> std::string_view { return artist.name; });

                // Picard use case: if we manage to match all artists in the "artist" tag (considered single-valued), and if no custom delimiter is hit, we use it as the display name
                // Otherwise, we reconstruct the string using a standard, hardcoded, join
                if (artistTag && strIsMatchingArtistNames(*artistTag, artistNames))
                {
                    if (!strIsContainingAny(*artistTag, artistTagDelimiters))
                        artistDisplayName = *artistTag;
                }

                if (artistDisplayName.empty())
                    artistDisplayName = core::stringUtils::joinStrings(artistNames, ", ");
            }

            return artistDisplayName;
        }
    } // namespace

    std::unique_ptr<IParser> createParser(ParserBackend parserBackend, ParserReadStyle parserReadStyle)
    {
        return std::make_unique<Parser>(parserBackend, parserReadStyle);
    }

    Parser::Parser(ParserBackend parserBackend, ParserReadStyle readStyle)
        : _parserBackend{ parserBackend }
        , _readStyle{ readStyle }
    {
        switch (_parserBackend)
        {
        case ParserBackend::TagLib:
            LMS_LOG(METADATA, INFO, "Using TagLib parser with read style = " << utils::readStyleToString(readStyle));
            break;

        case ParserBackend::AvFormat:
            LMS_LOG(METADATA, INFO, "Using AvFormat parser");
            break;
        }
    }

    std::unique_ptr<Track> Parser::parse(const std::filesystem::path& p, bool debug)
    {
        try
        {
            std::unique_ptr<ITagReader> tagReader;
            switch (_parserBackend)
            {
            case ParserBackend::TagLib:
                tagReader = std::make_unique<TagLibTagReader>(p, _readStyle, debug);
                break;

            case ParserBackend::AvFormat:
                tagReader = std::make_unique<AvFormatTagReader>(p, debug);
                break;
            }
            if (!tagReader)
                throw ParseException{ "Unhandled parser backend" };

            return parse(*tagReader);
        }
        catch (const Exception& e)
        {
            LMS_LOG(METADATA, ERROR, "File '" << p.string() << "': parsing failed");
            throw ParseException{};
        }
    }

    std::unique_ptr<Track> Parser::parse(const ITagReader& tagReader)
    {
        auto track{ std::make_unique<Track>() };

        track->audioProperties = tagReader.getAudioProperties();
        processTags(tagReader, *track);

        return track;
    }

    void Parser::processTags(const ITagReader& tagReader, Track& track)
    {
        track.hasCover = tagReader.hasEmbeddedCover();

        track.title = getTagValueAs<std::string>(tagReader, TagType::TrackTitle).value_or("");
        track.mbid = getTagValueAs<core::UUID>(tagReader, TagType::MusicBrainzTrackID);
        track.recordingMBID = getTagValueAs<core::UUID>(tagReader, TagType::MusicBrainzRecordingID);
        track.acoustID = getTagValueAs<core::UUID>(tagReader, TagType::AcoustID);
        track.position = getTagValueAs<std::size_t>(tagReader, TagType::TrackNumber); // May parse 'Number/Total', that's fine
        if (auto dateStr = getTagValueAs<std::string>(tagReader, TagType::Date))
        {
            if (const Wt::WDate date{ utils::parseDate(*dateStr) }; date.isValid())
            {
                track.date = date;
                track.year = date.year();
            }
            else
            {
                track.year = utils::parseYear(*dateStr);
            }
        }
        if (auto dateStr = getTagValueAs<std::string>(tagReader, TagType::OriginalReleaseDate))
        {
            if (const Wt::WDate date{ utils::parseDate(*dateStr) }; date.isValid())
            {
                track.originalDate = date;
                track.originalYear = date.year();
            }
            else
            {
                track.originalYear = utils::parseYear(*dateStr);
            }
        }
        if (auto dateStr = getTagValueAs<std::string>(tagReader, TagType::OriginalReleaseYear))
        {
            track.originalYear = utils::parseYear(*dateStr);
        }

        track.lyrics = getLyrics(tagReader); // no custom delimiter on lyrics
        track.comments = getTagValuesAs<std::string>(tagReader, TagType::Comment, {} /* no custom delimiter on comments */);
        track.copyright = getTagValueAs<std::string>(tagReader, TagType::Copyright).value_or("");
        track.copyrightURL = getTagValueAs<std::string>(tagReader, TagType::CopyrightURL).value_or("");
        track.replayGain = getTagValueAs<float>(tagReader, TagType::ReplayGainTrackGain);

        for (const std::string& userExtraTag : _userExtraTags)
        {
            visitTagValues(tagReader, userExtraTag, _defaultTagDelimiters, [&](std::string_view value) {
                value = core::stringUtils::stringTrim(value);
                if (!value.empty())
                    track.userExtraTags[userExtraTag].push_back(std::string{ value });
            });
        }

        track.genres = getTagValuesAs<std::string>(tagReader, TagType::Genre, _defaultTagDelimiters);
        track.moods = getTagValuesAs<std::string>(tagReader, TagType::Mood, _defaultTagDelimiters);
        track.groupings = getTagValuesAs<std::string>(tagReader, TagType::Grouping, _defaultTagDelimiters);
        track.languages = getTagValuesAs<std::string>(tagReader, TagType::Language, _defaultTagDelimiters);

        std::vector<std::string_view> artistDelimiters{};

        track.medium = getMedium(tagReader);
        track.artists = getArtists(tagReader, { TagType::Artists, TagType::Artist }, { TagType::ArtistSortOrder }, { TagType::MusicBrainzArtistID }, _artistTagDelimiters, _defaultTagDelimiters);
        track.artistDisplayName = computeArtistDisplayName(track.artists, getTagValueAs<std::string>(tagReader, TagType::Artist), _artistTagDelimiters);

        track.conductorArtists = getArtists(tagReader, { TagType::Conductors, TagType::Conductor }, { TagType::ConductorsSortOrder, TagType::ConductorSortOrder }, {}, _artistTagDelimiters, _defaultTagDelimiters);
        track.composerArtists = getArtists(tagReader, { TagType::Composers, TagType::Composer }, { TagType::ComposersSortOrder, TagType::ComposerSortOrder }, {}, _artistTagDelimiters, _defaultTagDelimiters);
        track.lyricistArtists = getArtists(tagReader, { TagType::Lyricists, TagType::Lyricist }, { TagType::LyricistsSortOrder, TagType::LyricistSortOrder }, {}, _artistTagDelimiters, _defaultTagDelimiters);
        track.mixerArtists = getArtists(tagReader, { TagType::Mixers, TagType::Mixer }, { TagType::MixersSortOrder, TagType::MixerSortOrder }, {}, _artistTagDelimiters, _defaultTagDelimiters);
        track.producerArtists = getArtists(tagReader, { TagType::Producers, TagType::Producer }, { TagType::ProducersSortOrder, TagType::ProducerSortOrder }, {}, _artistTagDelimiters, _defaultTagDelimiters);
        track.remixerArtists = getArtists(tagReader, { TagType::Remixers, TagType::Remixer }, { TagType::RemixersSortOrder, TagType::RemixerSortOrder }, {}, _artistTagDelimiters, _defaultTagDelimiters);
        track.performerArtists = getPerformerArtists(tagReader); // artistDelimiters not supported

        // If a file has date but no year, set it
        if (!track.year && track.date.isValid())
            track.year = track.date.year();

        // If a file has originalDate but no originalYear, set it
        if (!track.originalYear && track.originalDate.isValid())
            track.originalYear = track.originalDate.year();
    }

    std::optional<Medium> Parser::getMedium(const ITagReader& tagReader)
    {
        std::optional<Medium> medium;
        medium.emplace();

        medium->media = getTagValueAs<std::string>(tagReader, TagType::Media).value_or("");
        medium->name = getTagValueAs<std::string>(tagReader, TagType::DiscSubtitle).value_or("");
        medium->trackCount = getTagValueAs<std::size_t>(tagReader, TagType::TotalTracks);
        if (!medium->trackCount)
        {
            // totalTracks may be encoded as "position/count"
            if (const auto value{ getTagValueAs<std::string>(tagReader, TagType::TrackNumber) })
            {
                // Expecting 'Number/Total'
                const std::vector<std::string_view> strings{ core::stringUtils::splitString(*value, '/') };
                if (strings.size() == 2)
                    medium->trackCount = core::stringUtils::readAs<std::size_t>(strings[1]);
            }
        }
        // Expecting 'Number[/Total]'
        medium->position = getTagValueAs<std::size_t>(tagReader, TagType::DiscNumber);
        medium->release = getRelease(tagReader);
        medium->replayGain = getTagValueAs<float>(tagReader, TagType::ReplayGainAlbumGain);

        if (medium->isDefault())
            medium.reset();

        return medium;
    }

    std::optional<Release> Parser::getRelease(const ITagReader& tagReader)
    {
        std::optional<Release> release;

        auto releaseName{ getTagValueAs<std::string>(tagReader, TagType::Album) };
        if (!releaseName)
            return release;

        release.emplace();
        release->name = std::move(*releaseName);
        release->sortName = getTagValueAs<std::string>(tagReader, TagType::AlbumSortOrder).value_or("");
        release->artists = getArtists(tagReader, { TagType::AlbumArtists, TagType::AlbumArtist }, { TagType::AlbumArtistsSortOrder, TagType::AlbumArtistSortOrder }, { TagType::MusicBrainzReleaseArtistID }, _artistTagDelimiters, _defaultTagDelimiters);
        release->artistDisplayName = computeArtistDisplayName(release->artists, getTagValueAs<std::string>(tagReader, TagType::AlbumArtist), _artistTagDelimiters);
        release->mbid = getTagValueAs<core::UUID>(tagReader, TagType::MusicBrainzReleaseID);
        release->groupMBID = getTagValueAs<core::UUID>(tagReader, TagType::MusicBrainzReleaseGroupID);
        release->mediumCount = getTagValueAs<std::size_t>(tagReader, TagType::TotalDiscs);
        release->isCompilation = getTagValueAs<bool>(tagReader, TagType::Compilation).value_or(false);
        release->labels = getTagValuesAs<std::string>(tagReader, TagType::RecordLabel, _defaultTagDelimiters);
        if (!release->mediumCount)
        {
            // mediumCount may be encoded as "position/count"
            if (const auto value{ getTagValueAs<std::string>(tagReader, TagType::DiscNumber) })
            {
                // Expecting 'Number/Total'
                const std::vector<std::string_view> strings{ core::stringUtils::splitString(*value, '/') };
                if (strings.size() == 2)
                    release->mediumCount = core::stringUtils::readAs<std::size_t>(strings[1]);
            }
        }

        release->releaseTypes = getTagValuesAs<std::string>(tagReader, TagType::ReleaseType, _defaultTagDelimiters);

        return release;
    }
} // namespace lms::metadata