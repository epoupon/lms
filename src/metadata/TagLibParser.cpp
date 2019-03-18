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

boost::optional<std::string>
getPropertyValue(const TagLib::PropertyMap& properties, const std::string& key)
{
	boost::optional<std::string> res;

	const TagLib::StringList& values {properties[key]};
	if (values.isEmpty())
		return res;

	res = stringTrim(values.front().to8Bit(true));
	if (res->empty())
		res.reset();

	return res;
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
std::vector<std::string>
getMusicBrainzArtistID(const TagLib::PropertyMap& properties)
{
	boost::optional<std::string> value {getPropertyValue(properties, "MUSICBRAINZ_ARTISTID")};

	if (!value)
		return {};

	return splitAndTrimString(*value, "/");  // Picard separator is '/'
}

static
std::vector<Artist>
getArtists(const TagLib::PropertyMap& properties)
{
	std::vector<Artist> res;

	boost::optional<std::string> value;

	value = getPropertyValue(properties, "ARTISTS");
	if (value)
	{
		std::vector<std::string> artists {splitAndTrimString(*value, "/;")};  // Picard separator is '/'
		std::vector<std::string> artistsMBID {getMusicBrainzArtistID(properties)};

		for (std::size_t i {}; i < artists.size(); ++i)
			res.emplace_back(Artist{std::move(artists[i]), artistsMBID.size() == artists.size() ? std::move(artistsMBID[i]) : ""});

		return res;
	}

	value = getPropertyValue(properties, "ARTIST");
	if (value)
		res.emplace_back(Artist{std::move(*value), ""});

	return res;
}

static
boost::optional<Artist>
getAlbumArtist(const TagLib::PropertyMap& properties)
{
	boost::optional<Artist> res;

	boost::optional<std::string> value;

	value = getPropertyValue(properties, "ALBUMARTIST");
	if (!value)
		return res;

	res = Artist{std::move(*value), ""};

	value = getPropertyValue(properties, "MUSICBRAINZ_ALBUMARTISTID");
	if (value)
		res->musicBrainzArtistID = std::move(*value);

	return res;
}


static
boost::optional<Album>
getAlbum(const TagLib::PropertyMap& properties)
{
	boost::optional<Album> res;

	boost::optional<std::string> value {getPropertyValue(properties, "ALBUM")};
	if (!value)
		return res;

	res = Album{std::move(*value), ""};

	boost::optional<std::string> mbid {getPropertyValue(properties, "MUSICBRAINZ_ALBUMID")};
	if (mbid)
		res->musicBrainzAlbumID = std::move(*mbid);

	return res;
}

boost::optional<Track>
TagLibParser::parse(const boost::filesystem::path& p, bool debug)
{
	TagLib::FileRef f {p.string().c_str(),
			true, // read audio properties
			TagLib::AudioProperties::Average};

	if (f.isNull())
		return boost::none;

	if (!f.audioProperties())
		return boost::none;

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

				std::cout << "[" << tag << "] = " << joinStrings(strs, ",") << std::endl;
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
			else if (tag == "MUSICBRAINZ_TRACKID")
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
		track.albumArtist = getAlbumArtist(properties);
		track.album = getAlbum(properties);

	}

	return track;
}

} // namespace MetaData

