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

template<typename T>
std::vector<T>
getPropertyValuesFirstMatchAs(const TagLib::PropertyMap& properties, const std::vector<std::string_view>& keys)
{
	std::vector<T> res;

	for (std::string_view key : keys)
	{
		const TagLib::StringList& values {properties[std::string {key}]};
		if (values.isEmpty())
			continue;

		res.reserve(values.size());

		for (const auto& value : values)
		{
			auto val {StringUtils::readAs<T>(StringUtils::stringTrim(value.to8Bit(true)))};
			if (!val)
				continue;

			res.emplace_back(std::move(*val));
		}

		break;
	}

	return res;
}

template <typename T>
std::vector<T>
getPropertyValuesAs(const TagLib::PropertyMap& properties, const std::string& key)
{
	return getPropertyValuesFirstMatchAs<T>(properties, {std::move(key)});
}

static
std::vector<std::string>
splitAndTrimString(const std::string& str, std::string_view delimiters)
{
	std::vector<std::string> res;

	std::vector<std::string_view> strings {StringUtils::splitString(str, delimiters)};
	for (std::string_view s : strings)
		res.emplace_back(StringUtils::stringTrim(s));

	return res;
}

static
std::vector<Artist>
getArtists(const TagLib::PropertyMap& properties,
		const std::vector<std::string_view>& artistTagNames,
		const std::vector<std::string_view>& artistSortTagNames,
		const std::vector<std::string_view>& artistMBIDTagNames
		)
{
	const std::vector<std::string> artistNames {getPropertyValuesFirstMatchAs<std::string>(properties, artistTagNames)};
	if (artistNames.empty())
		return {};

	std::vector<Artist> artists;
	artists.reserve(artistNames.size());
	std::transform(std::cbegin(artistNames), std::cend(artistNames), std::back_inserter(artists),
		[&](const std::string& name) { return Artist {name}; });

	{
		const std::vector<std::string> artistSortNames {getPropertyValuesFirstMatchAs<std::string>(properties, artistSortTagNames)};
		if (artistSortNames.size() == artists.size())
		{
			for (std::size_t i {}; i < artistSortNames.size(); ++i)
				artists[i].sortName = artistSortNames[i];
		}
	}

	{
		const std::vector<UUID> artistsMBID {getPropertyValuesFirstMatchAs<UUID>(properties, artistMBIDTagNames)};

		if (artistNames.size() == artistsMBID.size())
		{
			for (std::size_t i {}; i < artistsMBID.size(); ++i)
				artists[i].musicBrainzArtistID = artistsMBID[i];
		}
	}


	return artists;
}

static
PerformerContainer
getPerformerArtists(const TagLib::PropertyMap& properties,
		const std::vector<std::string_view>& artistTagNames)
{
	PerformerContainer performers;

	// picard stores like this: (see https://picard-docs.musicbrainz.org/en/appendices/tag_mapping.html#performer)
	// PERFORMER: artist (role)
	if (const std::vector<std::string> artistNames {getPropertyValuesFirstMatchAs<std::string>(properties, artistTagNames)}; !artistNames.empty())
	{
		for (std::string_view entry : artistNames)
		{
			Utils::PerformerArtist performer {Utils::extractPerformerAndRole(entry)};
			StringUtils::capitalize(performer.role);
			performers[performer.role].push_back(std::move(performer.artist));
		}
	}
	// PERFORMER:role (MP3)
	else
	{
		for (const auto& [key, values] : properties)
		{
			if (key.startsWith("PERFORMER"))
			{
				std::string performerStr {key.to8Bit(true)};
				std::string role;
				if (const std::size_t rolePos {performerStr.find(':')}; rolePos != std::string::npos)
				{
					role = StringUtils::stringToLower(performerStr.substr(rolePos + 1, performerStr.size() - rolePos + 1));
					StringUtils::capitalize(role);
				}

				for (const auto& value : values)
					performers[role].push_back(Artist {value.to8Bit(true)});
			}
		}
	}

	return performers;
}

static
std::optional<Album>
getAlbum(const TagLib::PropertyMap& properties)
{
	std::vector<std::string> albumName {getPropertyValuesAs<std::string>(properties, "ALBUM")};
	if (albumName.empty())
		return std::nullopt;

	const std::vector<UUID> albumMBID {getPropertyValuesFirstMatchAs<UUID>(properties, {"MUSICBRAINZ_ALBUMID", "MUSICBRAINZ ALBUM ID", "MUSICBRAINZ/ALBUM ID"})};

	if (albumMBID.empty())
		return Album {std::move(albumName.front()), {}};
	else
		return Album {std::move(albumName.front()), albumMBID.front()};
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
TagLibParser::processTag(Track& track, const std::string& tag, const TagLib::StringList& values, bool debug)
{
	if (debug)
	{
		std::vector<std::string> strs;
		std::transform(values.begin(), values.end(), std::back_inserter(strs), [](const auto& value) { return value.to8Bit(true); });

		std::cout << "[" << tag << "] = " << StringUtils::joinStrings(strs, "*SEP*") << std::endl;
	}

	if (tag.empty() || values.isEmpty() || values.front().isEmpty())
		return;

	std::string value {StringUtils::stringTrim(values.front().to8Bit(true))};

	if (tag == "TITLE")
		track.title = value;
	else if (tag == "MUSICBRAINZ_RELEASETRACKID"
			|| tag == "MUSICBRAINZ RELEASE TRACK ID"
			|| tag == "MUSICBRAINZ/RELEASE TRACK ID")
	{
		track.trackMBID = UUID::fromString(value);
	}
	else if (tag == "MUSICBRAINZ_TRACKID"
			|| tag == "MUSICBRAINZ TRACK ID"
			|| tag == "MUSICBRAINZ/TRACK ID")
		track.recordingMBID = UUID::fromString(value);
	else if (tag == "ACOUSTID_ID")
		track.acoustID = UUID::fromString(value);
	else if (tag == "TRACKTOTAL")
	{
		auto totalTrack = StringUtils::readAs<std::size_t>(value);
		if (totalTrack)
			track.totalTrack = totalTrack;
	}
	else if (tag == "TRACKNUMBER")
	{
		// Expecting 'Number/Total'
		std::vector<std::string> strings {splitAndTrimString(value, "/")};

		if (!strings.empty())
		{
			track.trackNumber = StringUtils::readAs<std::size_t>(strings[0]);

			// Lower priority than TRACKTOTAL
			if (strings.size() > 1 && !track.totalTrack)
				track.totalTrack = StringUtils::readAs<std::size_t>(strings[1]);
		}
	}
	else if (tag == "DISCTOTAL")
	{
		auto totalDisc = StringUtils::readAs<std::size_t>(value);
		if (totalDisc)
			track.totalDisc = totalDisc;
	}
	else if (tag == "DISCNUMBER")
	{
		// Expecting 'Number/Total'
		std::vector<std::string_view> strings {StringUtils::splitString(value, "/")};
		if (!strings.empty())
		{
			track.discNumber = StringUtils::readAs<std::size_t>(strings[0]);

			// Lower priority than DISCTOTAL
			if (strings.size() > 1 && !track.totalDisc)
				track.totalDisc = StringUtils::readAs<std::size_t>(strings[1]);
		}
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
	else if (tag == "REPLAYGAIN_ALBUM_GAIN")
		track.albumReplayGain = StringUtils::readAs<float>(value);
	else if (tag == "REPLAYGAIN_TRACK_GAIN")
		track.trackReplayGain = StringUtils::readAs<float>(value);
	else if (tag == "DISCSUBTITLE" || tag == "SETSUBTITLE")
		track.discSubtitle = value;
	else if (_clusterTypeNames.find(tag) != _clusterTypeNames.end())
	{
		std::set<std::string> clusterNames;
		for (const auto& valueList : values)
		{
			const auto splittedValues {splitAndTrimString(valueList.to8Bit(true), "/,;")};

			for (const auto& value : splittedValues)
				clusterNames.insert(value);
		}

		if (!clusterNames.empty())
			track.clusters[tag] = clusterNames;
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
		track.audioStreams = {std::move(audioStream)};
	}

	TagLib::PropertyMap properties {f.file()->properties()};

	auto getAPETags = [&](const TagLib::APE::Tag* apeTag)
	{
		if (!apeTag)
			return;

		for (const auto& [name, values] : apeTag->properties())
		{
			if (debug)
				std::cout << "APE property: '" << name << "'" << std::endl;

			if (!properties.contains(name))
				properties.insert(name, values);
		}
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
				if (name.to8Bit().find("WM/") == 0 || properties.contains(name))
					continue;

				TagLib::StringList stringAttributeList;
				for (const auto& attribute : attributeList)
				{
					if (attribute.type() == TagLib::ASF::Attribute::AttributeTypes::UnicodeType)
						stringAttributeList.append(attribute.toString());
				}

				if (!stringAttributeList.isEmpty())
				{
					if (debug)
						std::cout << "ASF property: '" << name << "'" << std::endl;

					if (!properties.contains(name))
						properties.insert(name, stringAttributeList);
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
				properties.insert("DISCSUBTITLE", frameListMap["TSST"].front()->toString());
		}

		getAPETags(mp3File->APETag());
	}
	//MP4
	else if (TagLib::MP4::File* mp4File {dynamic_cast<TagLib::MP4::File*>(f.file())})
	{
		auto& coverItem {mp4File->tag()->itemListMap()["covr"]};
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

	for (const auto& [tag, values] : properties)
		processTag(track, tag.upper().to8Bit(true), values, debug);

	track.album = getAlbum(properties);
	track.artists = getArtists(properties, {"ARTISTS", "ARTIST"}, {"ARTISTSORT"}, {"MUSICBRAINZ_ARTISTID", "MUSICBRAINZ ARTIST ID", "MUSICBRAINZ/ARTIST ID"});
	track.albumArtists = getArtists(properties, {"ALBUMARTISTS", "ALBUMARTIST"}, {"ALBUMARTISTSSORT", "ALBUMARTISTSORT"}, {"MUSICBRAINZ_ALBUMARTISTID", "MUSICBRAINZ ALBUM ARTIST ID", "MUSICBRAINZ/ALBUM ARTIST ID"});
	track.conductorArtists = getArtists(properties, {"CONDUCTORS", "CONDUCTOR"}, {"CONDUCTORSSORT", "CONDUCTORSORT"}, {});
	track.composerArtists = getArtists(properties, {"COMPOSERS", "COMPOSER"}, {"COMPOSERSSORT", "COMPOSERSORT"}, {});
	track.lyricistArtists = getArtists(properties, {"LYRICISTS", "LYRICIST"}, {"LYRICISTSSORT", "LYRICISTSORT"}, {});
	track.mixerArtists = getArtists(properties, {"MIXERS", "MIXER"}, {"MIXERSSORT", "MIXERSORT"}, {});
	track.producerArtists = getArtists(properties, {"PRODUCERS", "PRODUCER"}, {"PRODUCERSSORT", "PRODUCERSORT"}, {});
	track.remixerArtists = getArtists(properties, {"REMIXERS", "REMIXER", "ModifiedBy"}, {"REMIXERSSORT", "REMIXERSORT"}, {});
	track.performerArtists = getPerformerArtists(properties, {"PERFORMERS", "PERFORMER"});

	return track;
}

} // namespace MetaData

