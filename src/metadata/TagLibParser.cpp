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

#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tpropertymap.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>

#include "logger/Logger.hpp"
#include "utils/Utils.hpp"

#include "TagLibParser.hpp"


namespace MetaData
{

bool
TagLibParser::parse(const boost::filesystem::path& p, Items& items)
{
	TagLib::FileRef f(p.string().c_str(),
			true, // read audio properties
			TagLib::AudioProperties::Average);

	if (f.isNull())
		return false;

	if (!f.audioProperties())
		return false;

	{
		TagLib::AudioProperties *properties = f.audioProperties();

		boost::posix_time::time_duration duration = boost::posix_time::seconds(properties->length());

		items.insert( std::make_pair(MetaData::Type::Duration, duration) );

		MetaData::AudioStream audioStream = { .desc = "", .bitRate = static_cast<std::size_t>(properties->bitrate() * 1000) };
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
		TagLib::PropertyMap tags = f.file()->properties();

      		for(TagLib::PropertyMap::ConstIterator itElem = tags.begin(); itElem != tags.end(); ++itElem)
		{
			const std::string tag = itElem->first.to8Bit(true);
			const TagLib::StringList &values = itElem->second;

			if (tag.empty() || values.isEmpty() || values.front().isEmpty())
				continue;

			// TODO validate MBID format

			if (tag == "ARTIST")
				items.insert( std::make_pair(MetaData::Type::Artist, stringTrim( values.front().to8Bit(true))));
			else if (tag == "ALBUM")
				items.insert( std::make_pair(MetaData::Type::Album, stringTrim( values.front().to8Bit(true))));
			else if (tag == "TITLE")
				items.insert( std::make_pair(MetaData::Type::Title, stringTrim( values.front().to8Bit(true))));
			else if (tag == "MUSICBRAINZ_RELEASETRACKID")
				items.insert( std::make_pair(MetaData::Type::MusicBrainzTrackID, stringTrim( values.front().to8Bit(true))));
			else if (tag == "MUSICBRAINZ_ARTISTID")
				items.insert( std::make_pair(MetaData::Type::MusicBrainzArtistID, stringTrim( values.front().to8Bit(true))));
			else if (tag == "MUSICBRAINZ_ALBUMID")
				items.insert( std::make_pair(MetaData::Type::MusicBrainzAlbumID, stringTrim( values.front().to8Bit(true))));
			else if (tag == "MUSICBRAINZ_TRACKID")
				items.insert( std::make_pair(MetaData::Type::MusicBrainzRecordingID, stringTrim( tags["MUSICBRAINZ_TRACKID"].front().to8Bit(true))));
			else if (tag == "TRACKNUMBER")
			{
				// Expecting 'Number/Total'
				auto strings = splitString(values.front().to8Bit(), "/");

				if (!strings.empty())
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
			else if (tag == "DISCNUMBER")
			{
				// Expecting 'Number/Total'
				auto strings = splitString(values.front().to8Bit(), "/");

				if (!strings.empty())
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
			else if (tag == "DATE")
			{
				boost::posix_time::ptime p;
				if (readAsPosixTime(values.front().to8Bit(), p))
					items.insert( std::make_pair(MetaData::Type::Date, p));
			}
			else if (tag == "ORIGINALDATE")
			{
				boost::posix_time::ptime p;
				if (readAsPosixTime(values.front().to8Bit(), p))
				{
					// Take priority on original year
					items.erase( MetaData::Type::OriginalDate );
					items.insert( std::make_pair(MetaData::Type::OriginalDate, p));
				}
			}
			else if (tag == "ORIGINALYEAR")
			{
				// lower priority than original date
				if (items.find(MetaData::Type::OriginalDate) == items.end())
				{
					boost::posix_time::ptime p;
					if (readAsPosixTime(values.front().to8Bit(), p))
						items.insert( std::make_pair(MetaData::Type::OriginalDate, p));
				}
			}
			else if (tag == "GENRE")
			{
				std::list<std::string> genreList;
				for (TagLib::StringList::ConstIterator itGenre = values.begin(); itGenre != values.end(); ++itGenre)
					genreList.push_back(itGenre->to8Bit(true));

				items.insert( std::make_pair(MetaData::Type::Genres, genreList) );
			}
			else if (tag == "METADATA_BLOCK_PICTURE")
			{
				// Only add once
				if (items.find(MetaData::Type::HasCover) == items.end())
					items.insert( std::make_pair(MetaData::Type::HasCover, true));
			}
		}
	}

	return true;
}

} // namespace MetaData

