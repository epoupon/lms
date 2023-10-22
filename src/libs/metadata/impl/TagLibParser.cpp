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

#include "TagLibParser.hpp"

#include <map>

#include <taglib/apetag.h>
#include <taglib/asffile.h>
#include <taglib/id3v2tag.h>
#include <taglib/fileref.h>
#include <taglib/flacfile.h>
#include <taglib/mp4file.h>
#include <taglib/mpcfile.h>
#include <taglib/mpegfile.h>
#include <taglib/opusfile.h>
#include <taglib/tag.h>
#include <taglib/tpropertymap.h>
#include <taglib/vorbisfile.h>
#include <taglib/wavpackfile.h>

#include "utils/IConfig.hpp"
#include "utils/Exception.hpp"
#include "utils/Logger.hpp"
#include "utils/Service.hpp"
#include "utils/String.hpp"
#include "Utils.hpp"

namespace MetaData
{

// TODO use string_views here for values
using TagMap = std::map<std::string, std::vector<std::string>>;

template<typename T>
std::vector<T>
getPropertyValuesFirstMatchAs(const TagMap& tags, std::initializer_list<std::string_view> keys)
{
	std::vector<T> res;

	for (std::string_view key : keys)
	{
		const auto itValues {tags.find(std::string {key})};
		if (itValues == std::cend(tags))
			continue;

		const std::vector<std::string>& values {itValues->second};
		if (values.empty())
			continue;

		res.reserve(values.size());

		for (const auto& value : values)
		{
			std::optional<T> val {StringUtils::readAs<T>(value)};
			if (!val)
				continue;

			res.emplace_back(std::move(*val));
		}

		break;
	}

	return res;
}

template <typename T>
std::optional<T>
getPropertyValueFirstMatchAs(const TagMap& tags, std::initializer_list<std::string_view> keys)
{
	std::optional<T> res;
	std::vector<T> values {getPropertyValuesFirstMatchAs<T>(tags, keys)};
	if (!values.empty())
		res = std::move(values.front());

	return res;
}

template <typename T>
std::vector<T>
getPropertyValuesAs(const TagMap& tags, std::string_view key)
{
	return getPropertyValuesFirstMatchAs<T>(tags, {key});
}

template <typename T>
std::optional<T>
getPropertyValueAs(const TagMap& tags, std::string_view key)
{
	return getPropertyValueFirstMatchAs<T>(tags, {key});
}

static
std::vector<std::string_view>
splitAndTrimString(std::string_view str, std::string_view delimiters)
{
	std::vector<std::string_view> strings {StringUtils::splitString(str, delimiters)};
	for (std::string_view& s : strings)
		s = StringUtils::stringTrim(s);

	return strings;
}

static
std::vector<Artist>
getArtists(const TagMap& tags,
		std::initializer_list<std::string_view> artistTagNames,
		std::initializer_list<std::string_view> artistSortTagNames,
		std::initializer_list<std::string_view> artistMBIDTagNames
		)
{
	const std::vector<std::string_view> artistNames {getPropertyValuesFirstMatchAs<std::string_view>(tags, artistTagNames)};
	if (artistNames.empty())
		return {};

	std::vector<Artist> artists;
	artists.reserve(artistNames.size());
	std::transform(std::cbegin(artistNames), std::cend(artistNames), std::back_inserter(artists),
		[&](std::string_view name) { return Artist {name}; });

	{
		const std::vector<std::string_view> artistSortNames {getPropertyValuesFirstMatchAs<std::string_view>(tags, artistSortTagNames)};
		if (artistSortNames.size() == artists.size())
		{
			for (std::size_t i {}; i < artistSortNames.size(); ++i)
				artists[i].sortName = artistSortNames[i];
		}
	}

	{
		const std::vector<UUID> artistsMBID {getPropertyValuesFirstMatchAs<UUID>(tags, artistMBIDTagNames)};

		if (artistNames.size() == artistsMBID.size())
		{
			for (std::size_t i {}; i < artistsMBID.size(); ++i)
				artists[i].mbid = artistsMBID[i];
		}
	}


	return artists;
}

static
PerformerContainer
getPerformerArtists(const TagMap& tags,
		std::initializer_list<std::string_view> artistTagNames)
{
	PerformerContainer performers;

	// picard stores like this: (see https://picard-docs.musicbrainz.org/en/appendices/tag_mapping.html#performer)
	// We may hit both styles for the same track
	// PERFORMER: artist (role)
	if (const std::vector<std::string_view> artistNames {getPropertyValuesFirstMatchAs<std::string_view>(tags, artistTagNames)}; !artistNames.empty())
	{
		for (std::string_view entry : artistNames)
		{
			Utils::PerformerArtist performer {Utils::extractPerformerAndRole(entry)};
			StringUtils::capitalize(performer.role);
			performers[performer.role].push_back(std::move(performer.artist));
		}
	}
	// PERFORMER:role (MP3)
	for (const auto& [key, values] : tags)
	{
		if (key.find("PERFORMER:") == 0)
		{
			std::string performerStr {key};
			std::string role;
			if (const std::size_t rolePos {performerStr.find(':')}; rolePos != std::string::npos)
			{
				role = StringUtils::stringToLower(performerStr.substr(rolePos + 1, performerStr.size() - rolePos + 1));
				StringUtils::capitalize(role);
			}

			for (const auto& value : values)
				performers[role].push_back(Artist {value});
		}
	}

	return performers;
}

static
std::optional<Release>
getRelease(const TagMap& tags)
{
	std::optional<Release> release;

	auto releaseName {getPropertyValueAs<std::string>(tags, "ALBUM")};
	if (!releaseName)
		return release;

	release.emplace();
	release->name = std::move(*releaseName);
	release->artistDisplayName = getPropertyValueAs<std::string_view>(tags, "ALBUMARTIST").value_or("");
	release->mbid = getPropertyValueFirstMatchAs<UUID>(tags, {"MUSICBRAINZ_ALBUMID", "MUSICBRAINZ ALBUM ID", "MUSICBRAINZ/ALBUM ID"});
	release->artists = getArtists(tags, {"ALBUMARTISTS", "ALBUMARTIST"}, {"ALBUMARTISTSSORT", "ALBUMARTISTSORT"}, {"MUSICBRAINZ_ALBUMARTISTID", "MUSICBRAINZ ALBUM ARTIST ID", "MUSICBRAINZ/ALBUM ARTIST ID"});
	release->mediumCount = getPropertyValueAs<std::size_t>(tags, "DISCTOTAL");
	if (!release->mediumCount)
	{
		// mediumCount may be encoded as "position/count"
		if (const auto value {getPropertyValueAs<std::string_view>(tags, "DISCNUMBER")})
		{
			// Expecting 'Number/Total'
			const std::vector<std::string_view> strings {StringUtils::splitString(*value, "/") };
			if (strings.size() == 2)
				release->mediumCount = StringUtils::readAs<std::size_t>(strings[1]);
		}
	}

	release->primaryType = getPropertyValueFirstMatchAs<MetaData::Release::PrimaryType>(tags, {"MUSICBRAINZ_ALBUMTYPE", "RELEASETYPE", "MUSICBRAINZ ALBUM TYPE", "MUSICBRAINZ/ALBUM TYPE"});
	if (release->primaryType)
	{
		const auto secondaryTypes {getPropertyValuesFirstMatchAs<MetaData::Release::SecondaryType>(tags, {"MUSICBRAINZ_ALBUMTYPE", "RELEASETYPE", "MUSICBRAINZ ALBUM TYPE", "MUSICBRAINZ/ALBUM TYPE"})};
		release->secondaryTypes.assign(std::cbegin(secondaryTypes), std::cend(secondaryTypes));
	}

	return release;
}

static
std::optional<Medium>
getMedium(const TagMap& tags)
{
	std::optional<Medium> medium;
	medium.emplace();

	medium->type = getPropertyValueAs<std::string>(tags, "MEDIA").value_or("");
	medium->name = getPropertyValueFirstMatchAs<std::string>(tags, {"DISCSUBTITLE", "SETSUBTITLE"}).value_or("");
	medium->trackCount = getPropertyValueAs<std::size_t>(tags, "TRACKTOTAL");
	if (!medium->trackCount)
	{
		// totalTracks may be encoded as "position/count"
		if (const auto value {getPropertyValueAs<std::string_view>(tags, "TRACKNUMBER")})
		{
			// Expecting 'Number/Total'
			const std::vector<std::string_view> strings {StringUtils::splitString(*value, "/") };
			if (strings.size() == 2)
				medium->trackCount = StringUtils::readAs<std::size_t>(strings[1]);
		}
	}
	// Expecting 'Number[/Total]'
	medium->position = getPropertyValueAs<std::size_t>(tags, "DISCNUMBER");
	medium->release = getRelease(tags);
	medium->replayGain = getPropertyValueAs<float>(tags, "REPLAYGAIN_ALBUM_GAIN");

	if (medium->type.empty()
			&& medium->name.empty()
			&& !medium->trackCount
			&& !medium->position
			&& !medium->release
			&& !medium->replayGain)
	{
		medium.reset();
	}

	return medium;
}

static
TagLib::AudioProperties::ReadStyle
readStyleToTagLibReadStyle(ParserReadStyle readStyle)
{
	switch (readStyle)
	{
		case ParserReadStyle::Fast: return TagLib::AudioProperties::ReadStyle::Fast;
		case ParserReadStyle::Average: return TagLib::AudioProperties::ReadStyle::Average;
		case ParserReadStyle::Accurate: return TagLib::AudioProperties::ReadStyle::Accurate;
	}

	throw LmsException {"Cannot convert read style"};
}

TagLibParser::TagLibParser(ParserReadStyle readStyle)
	: _readStyle {readStyleToTagLibReadStyle(readStyle)}
{
}

void
TagLibParser::processTag(Track& track, const std::string& tag, const std::vector<std::string>& values, bool debug)
{
	if (debug)
		std::cout << "[" << tag << "] = " << StringUtils::joinStrings(values, "*SEP*") << std::endl;

	if (tag.empty() || values.empty())
		return;

	std::string_view value {values.front()};

	if (tag == "TITLE")
		track.title = value;
	else if (tag == "MUSICBRAINZ_RELEASETRACKID"
			|| tag == "MUSICBRAINZ RELEASE TRACK ID"
			|| tag == "MUSICBRAINZ/RELEASE TRACK ID")
	{
		track.mbid = UUID::fromString(value);
	}
	else if (tag == "MUSICBRAINZ_TRACKID"
			|| tag == "MUSICBRAINZ TRACK ID"
			|| tag == "MUSICBRAINZ/TRACK ID")
		track.recordingMBID = UUID::fromString(value);
	else if (tag == "ACOUSTID_ID")
		track.acoustID = UUID::fromString(value);
	else if (tag == "TRACKNUMBER")
	{
		// Expecting 'Number/Total'
		track.position = StringUtils::readAs<std::size_t>(value);
	}
	else if (tag == "DATE")
	{
		// Higher priority than YEAR
		if (const Wt::WDate date {Utils::parseDate(value)}; date.isValid())
			track.date = date;
	}
	else if (tag == "YEAR" && !track.date.isValid())
	{
		// lower priority than DATE
		track.date = Utils::parseDate(value);
	}
	else if (tag == "ORIGINALDATE")
	{
		// Higher priority than ORIGINALYEAR
		if (const Wt::WDate date {Utils::parseDate(value)}; date.isValid())
			track.originalDate = date;
	}
	else if (tag == "ORIGINALYEAR" && !track.originalDate.isValid())
	{
		// Lower priority than ORIGINALDATE
		track.originalDate = Utils::parseDate(value);
	}
	else if (tag == "METADATA_BLOCK_PICTURE")
		track.hasCover = true;
	else if (tag == "COPYRIGHT")
		track.copyright = value;
	else if (tag == "COPYRIGHTURL")
		track.copyrightURL = value;
	else if (tag == "REPLAYGAIN_TRACK_GAIN")
		track.replayGain = StringUtils::readAs<float>(value);
	else if (tag == "ARTIST")
		track.artistDisplayName = value;
	else if (_clusterTypeNames.find(tag) != _clusterTypeNames.end())
	{
		std::set<std::string> clusterNames;
		for (std::string_view valueList : values)
		{
			const std::vector<std::string_view> splittedValues {splitAndTrimString(valueList, "/,;")};
			for (std::string_view value : splittedValues)
				clusterNames.insert(std::string {value});
		}

		if (!clusterNames.empty())
			track.tags[tag] = std::move(clusterNames);
	}
}

static
TagMap
constructTagMap(const TagLib::PropertyMap& properties)
{
	TagMap tagMap;

	for (const auto& [propertyName, propertyValues] : properties)
	{
		std::vector<std::string>& values {tagMap[propertyName.upper().to8Bit(true)]};
		for (const TagLib::String& propertyValue : propertyValues)
		{
			std::string trimedValue {StringUtils::stringTrim(propertyValue.to8Bit(true))};
			if (!trimedValue.empty())
				values.emplace_back(std::move(trimedValue));
		}
	}

	return tagMap;
}

static
void
mergeTagMaps(TagMap& dst, TagMap&& src)
{
	for (auto&& [tag, values] : src)
	{
		if (dst.find(tag) == std::cend(dst))
			dst[tag] = std::move(values);
	}
}

std::optional<Track>
TagLibParser::parse(const std::filesystem::path& p, bool debug)
{
	TagLib::FileRef f {p.string().c_str(),
		true, // read audio properties
		_readStyle};

	if (f.isNull())
	{
		LMS_LOG(METADATA, ERROR) << "File '" << p.string() << "': parsing failed";
		return std::nullopt;
	}

	if (!f.audioProperties())
	{
		LMS_LOG(METADATA, INFO) << "File '" << p.string() << "': no audio properties";
		return std::nullopt;
	}

	Track track;

	{
		const TagLib::AudioProperties *properties {f.audioProperties() };

		track.duration = std::chrono::milliseconds {properties->lengthInMilliseconds()};

		MetaData::AudioStream audioStream {static_cast<unsigned>(properties->bitrate() * 1000)};
		track.audioStreams = {audioStream};
	}

	TagMap tags {constructTagMap(f.file()->properties())};

	auto getAPETags = [&](const TagLib::APE::Tag* apeTag)
	{
		if (!apeTag)
			return;

		mergeTagMaps(tags, constructTagMap(apeTag->properties()));
	};

	// Not that good embedded pictures handling

	// WMA
	if (TagLib::ASF::File* asfFile {dynamic_cast<TagLib::ASF::File*>(f.file())})
	{
		const TagLib::ASF::Tag* tag {asfFile->tag()};
		if (tag)
		{
			if (tag->attributeListMap().contains("WM/Picture"))
				track.hasCover = true;

			for (const auto& [name, attributeList] : tag->attributeListMap())
			{
				std::string strName {StringUtils::stringToUpper(name.to8Bit(true))};
				if (strName.find("WM/") == 0 || tags.find(strName) != std::cend(tags))
					continue;

				std::vector<std::string> attributes;
				for (const auto& attribute : attributeList)
				{
					if (attribute.type() == TagLib::ASF::Attribute::AttributeTypes::UnicodeType)
						attributes.emplace_back(attribute.toString().to8Bit(true));
				}

				if (!attributes.empty())
				{
					if (debug)
						std::cout << "ASF property: '" << name << "'" << std::endl;

					tags.emplace(strName, std::move(attributes));
				}
			}
		}
	}
	// MP3
	else if (TagLib::MPEG::File* mp3File {dynamic_cast<TagLib::MPEG::File*>(f.file())})
	{
		if (mp3File->ID3v2Tag())
		{
			const auto& frameListMap {mp3File->ID3v2Tag()->frameListMap()};

			if (!frameListMap["APIC"].isEmpty())
				track.hasCover = true;
			if (!frameListMap["TSST"].isEmpty())
				tags["DISCSUBTITLE"] = {frameListMap["TSST"].front()->toString().to8Bit(true)};
		}

		getAPETags(mp3File->APETag());
	}
	//MP4
	else if (TagLib::MP4::File* mp4File {dynamic_cast<TagLib::MP4::File*>(f.file())})
	{
		TagLib::MP4::Item coverItem {mp4File->tag()->item("covr")};
		TagLib::MP4::CoverArtList coverArtList {coverItem.toCoverArtList()};
		if (!coverArtList.isEmpty())
			track.hasCover = true;
	}
	// MPC
	else if (TagLib::MPC::File* mpcFile {dynamic_cast<TagLib::MPC::File*>(f.file())})
	{
		getAPETags(mpcFile->APETag());
	}
	// WavPack
	else if (TagLib::WavPack::File* wavPackFile {dynamic_cast<TagLib::WavPack::File*>(f.file())})
	{
		getAPETags(wavPackFile->APETag());
	}
	// FLAC
	else if (TagLib::FLAC::File* flacFile {dynamic_cast<TagLib::FLAC::File*>(f.file())})
	{
		if (!flacFile->pictureList().isEmpty())
			track.hasCover = true;
	}
	else if (TagLib::Ogg::Vorbis::File* vorbisFile {dynamic_cast<TagLib::Ogg::Vorbis::File*>(f.file())})
	{
		if (!vorbisFile->tag()->pictureList().isEmpty())
			track.hasCover = true;
	}
	else if (TagLib::Ogg::Opus::File* opusFile {dynamic_cast<TagLib::Ogg::Opus::File*>(f.file())})
	{
		if (!opusFile->tag()->pictureList().isEmpty())
			track.hasCover = true;
	}

	track.medium = getMedium(tags);
	track.artists = getArtists(tags, {"ARTISTS", "ARTIST"}, {"ARTISTSORT"}, {"MUSICBRAINZ_ARTISTID", "MUSICBRAINZ ARTIST ID", "MUSICBRAINZ/ARTIST ID"});
	track.conductorArtists = getArtists(tags, {"CONDUCTORS", "CONDUCTOR"}, {"CONDUCTORSSORT", "CONDUCTORSORT"}, {});
	track.composerArtists = getArtists(tags, {"COMPOSERS", "COMPOSER"}, {"COMPOSERSSORT", "COMPOSERSORT"}, {});
	track.lyricistArtists = getArtists(tags, {"LYRICISTS", "LYRICIST"}, {"LYRICISTSSORT", "LYRICISTSORT"}, {});
	track.mixerArtists = getArtists(tags, {"MIXERS", "MIXER"}, {"MIXERSSORT", "MIXERSORT"}, {});
	track.producerArtists = getArtists(tags, {"PRODUCERS", "PRODUCER"}, {"PRODUCERSSORT", "PRODUCERSORT"}, {});
	track.remixerArtists = getArtists(tags, {"REMIXERS", "REMIXER", "ModifiedBy"}, {"REMIXERSSORT", "REMIXERSORT"}, {});
	track.performerArtists = getPerformerArtists(tags, {"PERFORMERS", "PERFORMER"});

	for (const auto& [tag, values] : tags)
		processTag(track, tag, values, debug);

	return track;
}

} // namespace MetaData

