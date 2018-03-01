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

#include <boost/algorithm/string.hpp>

#include "av/AvInfo.hpp"

#include "utils/Logger.hpp"
#include "utils/Utils.hpp"

#include "AvFormat.hpp"


namespace MetaData
{

AvFormat::AvFormat(const std::map<std::string, std::string>& clusterMap)
: _clusterMap(clusterMap)
{
}

boost::optional<Items>
AvFormat::parse(const boost::filesystem::path& p)
{
	Items items;

	Av::MediaFile mediaFile(p);

	if (!mediaFile.open())
		return boost::none;

	if (!mediaFile.scan())
		return boost::none;

	// Stream info
	{
		std::vector<AudioStream> audioStreams;

		std::vector<Av::Stream> streams = mediaFile.getStreams(Av::Stream::Type::Audio);

		for (Av::Stream& stream : streams)
		{
			AudioStream audioStream;
			audioStream.desc = stream.desc;
			audioStream.bitRate = stream.bitrate;

			audioStreams.push_back(audioStream);
		}

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
#if 0
		std::cout << "TAG = " << tag << ", VAL = " << value << std::endl;
#endif
		if (tag == "ARTIST")
			items.insert( std::make_pair(MetaData::Type::Artist, stringTrim( stringToUTF8(value)) ));
		else if (tag == "ALBUM")
			items.insert( std::make_pair(MetaData::Type::Album, stringTrim( stringToUTF8(value)) ));
		else if (tag == "TITLE")
			items.insert( std::make_pair(MetaData::Type::Title, stringTrim( stringToUTF8(value)) ));
		else if (tag == "TRACK")
		{
			// Expecting 'Number/Total'
			auto strings = splitString(value, "/");

			if (strings.size() > 0)
			{
				std::size_t number;
				if (readAs<std::size_t>(strings[0], number))
					items.insert( std::make_pair(MetaData::Type::TrackNumber, number ));

				if (strings.size() > 1)
				{
					std::size_t totalNumber;
					if (readAs<std::size_t>(strings[1], totalNumber))
						items.insert( std::make_pair(MetaData::Type::TotalTrack, totalNumber ));
				}
			}
		}
		else if (tag == "DISC")
		{
			// Expecting 'Number/Total'
			auto strings = splitString(value, "/");

			if (strings.size() > 0)
			{
				std::size_t number;
				if (readAs<std::size_t>(strings[0], number))
					items.insert( std::make_pair(MetaData::Type::DiscNumber, number ));

				if (strings.size() > 1)
				{
					std::size_t totalNumber;
					if (readAs<std::size_t>(strings[1], totalNumber))
						items.insert( std::make_pair(MetaData::Type::TotalDisc, totalNumber ));
				}
			}
		}
		else if (tag == "DATE"
			|| tag == "YEAR"
			|| tag == "WM/Year")
		{
			boost::posix_time::ptime p;
			if (readAsPosixTime(value, p))
				items.insert( std::make_pair(MetaData::Type::Date, p));
		}
		else if (tag == "TDOR"	// Original release time (ID3v2 2.4)
			|| tag == "TORY")	// Original release year
		{
			boost::posix_time::ptime p;
			if (readAsPosixTime(value, p))
				items.insert( std::make_pair(MetaData::Type::OriginalDate, p));
		}
		else if (tag == "MUSICBRAINZ ARTIST ID"
			|| tag == "MUSICBRAINZ_ARTISTID")
		{
			items.insert( std::make_pair(MetaData::Type::MusicBrainzArtistID, stringTrim( stringToUTF8(value)) ));
		}
		else if (tag == "MUSICBRAINZ ALBUM ID"
			|| tag == "MUSICBRAINZ_ALBUMID")
		{
			items.insert( std::make_pair(MetaData::Type::MusicBrainzAlbumID, stringTrim( stringToUTF8(value)) ));
		}
		else if (tag == "MUSICBRAINZ RELEASE TRACK ID"
			|| tag == "MUSICBRAINZ_RELEASETRACKID"
			|| tag == "MUSICBRAINZ_TRACKID")
		{
			items.insert( std::make_pair(MetaData::Type::MusicBrainzTrackID, stringTrim( stringToUTF8(value)) ));
		}
		else if (tag == "ACOUSTID ID")
		{
			items.insert( std::make_pair(MetaData::Type::AcoustID, stringTrim( stringToUTF8(value)) ));
		}
		else if (_clusterMap.find(tag) != _clusterMap.end())
		{
			std::vector<std::string> clusterNames = splitString(value, ";,\\");

			if (!clusterNames.empty())
			{
				clusters[_clusterMap[tag]] = std::set<std::string>(clusterNames.begin(), clusterNames.end());
			}
		}

	}

	if (!clusters.empty())
		items.insert( std::make_pair(MetaData::Type::Clusters, clusters) );

	return items;
}

} // namespace MetaData

