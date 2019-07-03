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

#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tpropertymap.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>

#include "utils/Logger.hpp"
#include "utils/Utils.hpp"

namespace MetaData
{

std::vector<std::string>
getPropertyValuesFirstMatch(const TagLib::PropertyMap& properties, const std::set<std::string>& keys)
{
	std::vector<std::string> res;

	for (const std::string& key : keys)
	{
		const TagLib::StringList& values {properties[key]};
		if (values.isEmpty())
			continue;

		res.reserve(values.size());
		std::transform(std::cbegin(values), std::cend(values), std::back_inserter(res), [](const auto& value) { return stringTrim(value.to8Bit(true)); });

		break;
	}

	return res;
}

std::vector<std::string>
getPropertyValues(const TagLib::PropertyMap& properties, const std::string& key)
{
	return getPropertyValuesFirstMatch(properties, {std::move(key)});
}

static
std::vector<std::string>
splitAndTrimString(const std::string& str, const std::string& delimiters)
{
	std::vector<std::string> res;

	std::vector<std::string> strings {splitString(str, delimiters)};
	for (const std::string& s : strings)
		res.emplace_back(stringTrim(s));

	return res;
}

static
std::vector<Artist>
getArtists(const TagLib::PropertyMap& properties)
{
	std::vector<Artist> res;

	std::vector<std::string> artistNames {getPropertyValues(properties, "ARTISTS")};
	if (artistNames.empty())
		artistNames = getPropertyValues(properties, "ARTIST");

	if (artistNames.empty())
		return res;

	const std::vector<std::string> artistsMBID {getPropertyValuesFirstMatch(properties, {"MUSICBRAINZ_ARTISTID", "MUSICBRAINZ ARTIST ID"})};

	if (artistNames.size() == artistsMBID.size())
	{
		std::transform(std::cbegin(artistNames), std::cend(artistNames), std::cbegin(artistsMBID), std::back_inserter(res),
				[&](const std::string& name, const std::string& mbid) { return Artist{name, mbid}; });
	}
	else
	{
		std::transform(std::cbegin(artistNames), std::cend(artistNames), std::back_inserter(res),
				[&](const std::string& name) { return Artist{name, ""}; });
	}

	return res;
}

static
std::vector<Artist>
getAlbumArtists(const TagLib::PropertyMap& properties)
{
	std::vector<Artist> res;

	std::vector<std::string> artistNames {getPropertyValues(properties, "ALBUMARTIST")};
	if (artistNames.empty())
		return res;

	const std::vector<std::string> artistsMBID {getPropertyValuesFirstMatch(properties, {"MUSICBRAINZ_ALBUMARTISTID", "MUSICBRAINZ ALBUM ARTIST ID"})};

	if (artistNames.size() == artistsMBID.size())
	{
		std::transform(std::cbegin(artistNames), std::cend(artistNames), std::cbegin(artistsMBID), std::back_inserter(res),
				[&](const std::string& name, const std::string& mbid) { return Artist{name, mbid}; });
	}
	else
	{
		std::transform(std::cbegin(artistNames), std::cend(artistNames), std::back_inserter(res),
				[&](const std::string& name) { return Artist{name, ""}; });
	}

	return res;
}

static
boost::optional<Album>
getAlbum(const TagLib::PropertyMap& properties)
{
	boost::optional<Album> res;

	std::vector<std::string> albumName {getPropertyValues(properties, "ALBUM")};
	if (albumName.empty())
		return res;

	std::vector<std::string> albumMBID {getPropertyValuesFirstMatch(properties, {"MUSICBRAINZ_ALBUMID", "MUSICBRAINZ ALBUM ID"})};

	res = Album{std::move(albumName.front()), ""};

	if (!albumMBID.empty())
		res->musicBrainzAlbumID = std::move(albumMBID.front());

	return res;
}

boost::optional<Track>
TagLibParser::parse(const boost::filesystem::path& p, bool debug)
{
	TagLib::FileRef f {p.string().c_str(),
			true, // read audio properties
			TagLib::AudioProperties::Fast};

	if (f.isNull())
	{
		LMS_LOG(METADATA, ERROR) << "File '" << p.string() << "': parsing failed";
		return boost::none;
	}

	if (!f.audioProperties())
	{
		LMS_LOG(METADATA, INFO) << "File '" << p.string() << "': no audio properties";
		return boost::none;
	}

	Track track;

	{
		const TagLib::AudioProperties *properties {f.audioProperties() };

		track.duration = std::chrono::milliseconds {properties->length() * 1000};

		MetaData::AudioStream audioStream {.bitRate = static_cast<unsigned>(properties->bitrate() * 1000)};
		track.audioStreams = {std::move(audioStream)};
	}

	// Not that good embedded pictures handling

	// MP3
	if (TagLib::MPEG::File *mp3File {dynamic_cast<TagLib::MPEG::File*>(f.file())})
	{
		if (mp3File->ID3v2Tag())
		{
			if (!mp3File->ID3v2Tag()->frameListMap()["APIC"].isEmpty())
				track.hasCover = true;
		}
	}

	if (f.tag())
	{
		MetaData::Clusters clusters;
		const TagLib::PropertyMap& properties {f.file()->properties()};

      		for(const auto& property : properties)
		{
			const std::string tag {property.first.upper().to8Bit(true)};
			const TagLib::StringList& values {property.second};

			// TODO validate MBID format
			if (debug)
			{
				std::vector<std::string> strs;
				std::transform(values.begin(), values.end(), std::back_inserter(strs), [](const auto& value) { return value.to8Bit(true); });

				std::cout << "[" << tag << "] = " << joinStrings(strs, "*SEP*") << std::endl;
			}

			if (tag.empty() || values.isEmpty() || values.front().isEmpty())
				continue;

			std::string value {stringTrim(values.front().to8Bit(true))};

			if (tag == "TITLE")
				track.title = value;
			else if (tag == "MUSICBRAINZ_RELEASETRACKID"
				|| tag == "MUSICBRAINZ RELEASE TRACK ID")
			{
				track.musicBrainzTrackID = value;
			}
			else if (tag == "MUSICBRAINZ_TRACKID"
				|| tag == "MUSICBRAINZ TRACK ID")
				track.musicBrainzRecordID = value;
			else if (tag == "ACOUSTID_ID")
				track.acoustID = value;
			else if (tag == "TRACKTOTAL")
			{
				auto totalTrack = readAs<std::size_t>(value);
				if (totalTrack)
					track.totalTrack = totalTrack;
			}
			else if (tag == "TRACKNUMBER")
			{
				// Expecting 'Number/Total'
				std::vector<std::string> strings {splitAndTrimString(value, "/")};

				if (!strings.empty())
				{
					track.trackNumber = readAs<std::size_t>(strings[0]);

					// Lower priority than TRACKTOTAL
					if (strings.size() > 1 && !track.totalTrack)
						track.totalTrack = readAs<std::size_t>(strings[1]);
				}
			}
			else if (tag == "DISCTOTAL")
			{
				auto totalDisc = readAs<std::size_t>(value);
				if (totalDisc)
					track.totalDisc = totalDisc;
			}
			else if (tag == "DISCNUMBER")
			{
				// Expecting 'Number/Total'
				std::vector<std::string> strings {splitString(value, "/")};

				if (!strings.empty())
				{
					track.discNumber = readAs<std::size_t>(strings[0]);

					// Lower priority than DISCTOTAL
					if (strings.size() > 1 && !track.totalDisc)
						track.totalDisc = readAs<std::size_t>(strings[1]);
				}
			}
			else if (tag == "DATE")
				track.year = readAs<int>(value);
			else if (tag == "ORIGINALDATE" && !track.originalYear)
			{
				// Lower priority than ORIGINALYEAR
				track.originalYear = readAs<int>(value);
			}
			else if (tag == "ORIGINALYEAR")
			{
				// Higher priority than ORIGINALDATE
				auto originalYear = readAs<int>(value);
				if (originalYear)
					track.originalYear = originalYear;
			}
			else if (tag == "METADATA_BLOCK_PICTURE")
				track.hasCover = true;
			else if (tag == "COPYRIGHT")
				track.copyright = value;
			else if (tag == "COPYRIGHTURL")
				track.copyrightURL = value;
			else if (_clusterTypeNames.find(tag) != _clusterTypeNames.end())
			{
				std::set<std::string> clusterNames;
				for (const auto& valueList : values)
				{
					auto values = splitAndTrimString(valueList.to8Bit(true), "/,;");

					for (const auto& value : values)
						clusterNames.insert(value);
				}

				if (!clusterNames.empty())
					track.clusters[tag] = clusterNames;
			}
		}

		track.artists = getArtists(properties);
		track.albumArtists = getAlbumArtists(properties);
		track.album = getAlbum(properties);

	}

	return track;
}

} // namespace MetaData

