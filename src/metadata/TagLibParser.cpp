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

boost::optional<Items>
TagLibParser::parse(const boost::filesystem::path& p, bool debug)
{
	TagLib::FileRef f(p.string().c_str(),
			true, // read audio properties
			TagLib::AudioProperties::Average);

	if (f.isNull())
		return boost::none;

	if (!f.audioProperties())
		return boost::none;

	Items items;

	{
		TagLib::AudioProperties *properties = f.audioProperties();

		std::chrono::milliseconds duration(properties->length() * 1000);
		items.insert( std::make_pair(MetaData::Type::Duration, duration) );

		MetaData::AudioStream audioStream = { .bitRate = static_cast<std::size_t>(properties->bitrate() * 1000) };
		items.insert( std::make_pair(MetaData::Type::AudioStreams, std::vector<MetaData::AudioStream>(1, audioStream ) ));
	}

	// Not that good embedded pictures handling

	// MP3
	if (TagLib::MPEG::File *mp3File = dynamic_cast<TagLib::MPEG::File*>(f.file()))
	{
		if (mp3File->ID3v2Tag())
		{
			if (!mp3File->ID3v2Tag()->frameListMap()["APIC"].isEmpty())
				items.insert( std::make_pair(MetaData::Type::HasCover, true));
		}
	}

	if (f.tag())
	{
		MetaData::Clusters clusters;
		TagLib::PropertyMap properties = f.file()->properties();

      		for(auto property : properties)
		{
			const std::string tag = property.first.upper().to8Bit(true);
			const TagLib::StringList& values = property.second;

			if (tag.empty() || values.isEmpty() || values.front().isEmpty())
				continue;

			// TODO validate MBID format

			if (debug)
			{
				std::vector<std::string> strs;
				for (auto value : values)
					strs.push_back(values.front().to8Bit(true));

				std::cout << "[" << tag << "] = " << joinStrings(strs, ",") << std::endl;
			}

			if (tag == "ARTIST")
				items.insert( std::make_pair(MetaData::Type::Artist, stringTrim( values.front().to8Bit(true))));
			else if (tag == "ALBUM")
				items.insert( std::make_pair(MetaData::Type::Album, stringTrim( values.front().to8Bit(true))));
			else if (tag == "TITLE")
				items.insert( std::make_pair(MetaData::Type::Title, stringTrim( values.front().to8Bit(true))));
			else if (tag == "MUSICBRAINZ_RELEASETRACKID"
				|| tag == "MUSICBRAINZ RELEASE TRACK ID")
			{
				items.insert( std::make_pair(MetaData::Type::MusicBrainzTrackID, stringTrim( values.front().to8Bit(true))));
			}
			else if (tag == "MUSICBRAINZ_ARTISTID")
				items.insert( std::make_pair(MetaData::Type::MusicBrainzArtistID, stringTrim( values.front().to8Bit(true))));
			else if (tag == "MUSICBRAINZ_ALBUMID")
				items.insert( std::make_pair(MetaData::Type::MusicBrainzAlbumID, stringTrim( values.front().to8Bit(true))));
			else if (tag == "MUSICBRAINZ_TRACKID")
				items.insert( std::make_pair(MetaData::Type::MusicBrainzRecordingID, stringTrim( values.front().to8Bit(true))));
			else if (tag == "ACOUSTID_ID")
				items.insert( std::make_pair(MetaData::Type::AcoustID, stringTrim( values.front().to8Bit(true))));
			else if (tag == "TRACKTOTAL")
			{
				auto totalTrack = readAs<std::size_t>(values.front().to8Bit(true));
				if (totalTrack)
					items[MetaData::Type::TotalTrack] = *totalTrack;
			}
			else if (tag == "TRACKNUMBER")
			{
				// Expecting 'Number/Total'
				auto strings = splitString(values.front().to8Bit(), "/");

				if (!strings.empty())
				{
					auto number = readAs<std::size_t>(strings[0]);
					if (number)
						items.insert( std::make_pair(MetaData::Type::TrackNumber, *number ));

					// Lower priority than TRACKTOTAL
					if (strings.size() > 1 && items.find(MetaData::Type::TotalTrack) == items.end())
					{
						auto totalTrack = readAs<std::size_t>(strings[1]);
						if (totalTrack)
							items[MetaData::Type::TotalTrack] = *totalTrack;
					}
				}
			}
			else if (tag == "DISCTOTAL")
			{
				auto totalDisc = readAs<std::size_t>(values.front().to8Bit(true));
				if (totalDisc)
					items[MetaData::Type::TotalDisc] = *totalDisc;
			}
			else if (tag == "DISCNUMBER")
			{
				// Expecting 'Number/Total'
				auto strings = splitString(values.front().to8Bit(), "/");

				if (!strings.empty())
				{
					auto number = readAs<std::size_t>(strings[0]);
					if (number)
						items.insert( std::make_pair(MetaData::Type::DiscNumber, *number));

					// Lower priority than DISCTOTAL
					if (strings.size() > 1 && items.find(MetaData::Type::TotalDisc) == items.end())
					{
						auto totalDisc = readAs<std::size_t>(strings[1]);
						if (totalDisc)
							items[MetaData::Type::TotalDisc] = *totalDisc;
					}
				}
			}
			else if (tag == "DATE")
			{
				auto timePoint = readAs<int>(values.front().to8Bit());
				if (timePoint)
					items.insert( std::make_pair(MetaData::Type::Year, *timePoint));
			}
			else if (tag == "ORIGINALDATE")
			{
				// Lower priority than original year
				if (items.find(MetaData::Type::OriginalYear) == items.end())
				{
					auto timePoint = readAs<int>(values.front().to8Bit());
					if (timePoint)
						items[MetaData::Type::OriginalYear] = *timePoint;
				}
			}
			else if (tag == "ORIGINALYEAR")
			{
				auto timePoint = readAs<int>(values.front().to8Bit());
				if (timePoint)
				{
					// Take priority on original year
					items[MetaData::Type::OriginalYear] = *timePoint;
				}
			}
			else if (tag == "METADATA_BLOCK_PICTURE")
			{
				// Only add once
				if (items.find(MetaData::Type::HasCover) == items.end())
					items.insert( std::make_pair(MetaData::Type::HasCover, true));
			}
			else if (tag == "COPYRIGHT")
			{
				items.insert(std::make_pair(MetaData::Type::Copyright, values.front().to8Bit()));
			}
			else if (tag == "COPYRIGHTURL")
			{
				items.insert(std::make_pair(MetaData::Type::CopyrightURL, values.front().to8Bit()));
			}
			else if (_clusterTypeNames.find(tag) != _clusterTypeNames.end())
			{
				std::set<std::string> clusterNames;
				for (const auto& value : values)
					clusterNames.insert(value.to8Bit(true));

				if (!clusterNames.empty())
					clusters[tag] = clusterNames;
			}
		}

		if (!clusters.empty())
			items.insert( std::make_pair(MetaData::Type::Clusters, clusters) );
	}

	return items;
}

} // namespace MetaData

