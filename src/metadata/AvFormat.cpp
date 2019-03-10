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

#include "AvFormat.hpp"

#include <boost/algorithm/string.hpp>

#include "av/AvInfo.hpp"

#include "utils/Logger.hpp"
#include "utils/Utils.hpp"

namespace MetaData
{

boost::optional<Items>
AvFormat::parse(const boost::filesystem::path& p, bool debug)
{
	Items items;

	try
	{
		Av::MediaFile mediaFile(p);

		// Stream info
		{
			std::vector<AudioStream> audioStreams;

			auto streams = mediaFile.getStreamInfo();

			for (auto stream : streams)
				audioStreams.push_back( {.bitRate = stream.bitrate } );

			if (!audioStreams.empty())
				items.insert( std::make_pair(MetaData::Type::AudioStreams, audioStreams));
		}

		// Duration
		items.insert( std::make_pair(MetaData::Type::Duration, mediaFile.getDuration() ));

		// Cover
		items.insert( std::make_pair(MetaData::Type::HasCover, mediaFile.hasAttachedPictures()));

		// Embedded MetaData
		// Make sure to convert strings into UTF-8

		MetaData::Clusters clusters;

		std::map<std::string, std::string> metadataMap = mediaFile.getMetaData();
		for (auto metadata : metadataMap)
		{
			const std::string tag = boost::to_upper_copy<std::string>(metadata.first);
			const std::string value = metadata.second;

			if (debug)
				std::cout << "TAG = " << tag << ", VAL = " << value << std::endl;

			if (tag == "ARTIST")
			{
				if (items.find(MetaData::Type::Artists) == items.end())
					items[MetaData::Type::Artists] = std::set<std::string>{ stringTrim( value) };
			}
			else if (tag == "ARTISTS")
			{
				std::vector<std::string> strings {splitString(value, "/;")};  // Picard separator is '/'

				std::set<std::string> artists;
				for (const std::string& string : strings)
					artists.insert(stringTrim(string));

				items[MetaData::Type::Artists] = std::move(artists);
			}
			else if (tag == "ALBUM")
				items.insert( std::make_pair(MetaData::Type::Album, stringTrim( value) ));
			else if (tag == "TITLE")
				items.insert( std::make_pair(MetaData::Type::Title, stringTrim( value) ));
			else if (tag == "TRACK")
			{
				// Expecting 'Number/Total'
				auto strings = splitString(value, "/");

				if (strings.size() > 0)
				{
					auto number = readAs<std::size_t>(strings[0]);
					if (number)
						items.insert( std::make_pair(MetaData::Type::TrackNumber, *number ));

					if (strings.size() > 1)
					{
						auto totalNumber = readAs<std::size_t>(strings[1]);
						if (totalNumber)
							items.insert( std::make_pair(MetaData::Type::TotalTrack, *totalNumber ));
					}
				}
			}
			else if (tag == "DISC")
			{
				// Expecting 'Number/Total'
				auto strings = splitString(value, "/");

				if (strings.size() > 0)
				{
					auto number = readAs<std::size_t>(strings[0]);
					if (number)
						items.insert( std::make_pair(MetaData::Type::DiscNumber, *number ));

					if (strings.size() > 1)
					{
						auto totalNumber = readAs<std::size_t>(strings[1]);
						if (totalNumber)
							items.insert( std::make_pair(MetaData::Type::TotalDisc, *totalNumber ));
					}
				}
			}
			else if (tag == "DATE"
					|| tag == "YEAR"
					|| tag == "WM/Year")
			{
				auto date = readAs<int>(value);
				if (date)
					items.insert(std::make_pair(MetaData::Type::Year, *date));
			}
			else if (tag == "TDOR"	// Original release time (ID3v2 2.4)
					|| tag == "TORY")	// Original release year
			{
				auto date = readAs<int>(value);
				if (date)
					items.insert(std::make_pair(MetaData::Type::OriginalYear, *date));
			}
			else if (tag == "MUSICBRAINZ ARTIST ID"
					|| tag == "MUSICBRAINZ_ARTISTID")
			{
				std::vector<std::string> strings {splitString(value, "/")};

				std::set<std::string> mbids;
				for (const std::string& string : strings)
					mbids.insert(stringTrim(string));

				items[MetaData::Type::MusicBrainzArtistID] = std::move(mbids);
			}
			else if (tag == "MUSICBRAINZ ALBUM ID"
					|| tag == "MUSICBRAINZ_ALBUMID")
			{
				items.insert( std::make_pair(MetaData::Type::MusicBrainzAlbumID, stringTrim(value)) );
			}
			else if (tag == "MUSICBRAINZ RELEASE TRACK ID"
					|| tag == "MUSICBRAINZ_RELEASETRACKID"
					|| tag == "MUSICBRAINZ_TRACKID")
			{
				items.insert( std::make_pair(MetaData::Type::MusicBrainzTrackID, stringTrim(value)) );
			}
			else if (tag == "ACOUSTID ID")
			{
				items.insert( std::make_pair(MetaData::Type::AcoustID, stringTrim(value)) );
			}
			else if (_clusterTypeNames.find(tag) != _clusterTypeNames.end())
			{
				std::vector<std::string> clusterNames = splitString(value, "/,;");

				if (!clusterNames.empty())
				{
					clusters[tag] = std::set<std::string>(clusterNames.begin(), clusterNames.end());
				}
			}

		}

		if (!clusters.empty())
			items.insert( std::make_pair(MetaData::Type::Clusters, clusters) );
	}
	catch(Av::MediaFileException& e)
	{
		return boost::none;
	}

	return items;
}

} // namespace MetaData

