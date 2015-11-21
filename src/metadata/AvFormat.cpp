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

#include "logger/Logger.hpp"
#include "utils/Utils.hpp"
#include <boost/algorithm/string.hpp>

#include "av/AvInfo.hpp"


namespace MetaData
{

bool
AvFormat::parse(const boost::filesystem::path& p, Items& items)
{

	Av::MediaFile mediaFile(p);

	if (!mediaFile.open())
		return false;

	if (!mediaFile.scan())
		return false;

	std::map<std::string, std::string> metadata = mediaFile.getMetaData();

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

	{
		std::vector<VideoStream> videoStreams;

		std::vector<Av::Stream> streams = mediaFile.getStreams(Av::Stream::Type::Video);

		for (Av::Stream& stream : streams)
		{
			VideoStream videoStream;
			videoStream.desc = stream.desc;
			videoStream.bitRate = stream.bitrate;

			videoStreams.push_back(videoStream);
		}

		if (!videoStreams.empty())
			items.insert( std::make_pair(MetaData::Type::VideoStreams, videoStreams));
	}

	{
		std::vector<SubtitleStream> subtitleStreams;

		std::vector<Av::Stream> streams = mediaFile.getStreams(Av::Stream::Type::Subtitle);

		for (Av::Stream& stream : streams)
		{
			SubtitleStream subtitleStream;
			subtitleStream.desc = stream.desc;

			subtitleStreams.push_back(subtitleStream);
		}

		if (!subtitleStreams.empty())
			items.insert( std::make_pair(MetaData::Type::SubtitleStreams, subtitleStreams));
	}

	// Duration
	items.insert( std::make_pair(MetaData::Type::Duration, mediaFile.getDuration() ));

	// Cover
	items.insert( std::make_pair(MetaData::Type::HasCover, mediaFile.hasAttachedPictures()));

	// Embedded MetaData
	// Make sure to convert strings into UTF-8
	std::map<std::string, std::string>::const_iterator it;
	for (it = metadata.begin(); it != metadata.end(); ++it)
	{
		if (boost::iequals(it->first, "artist"))
			items.insert( std::make_pair(MetaData::Type::Artist, stringTrim( stringToUTF8(it->second)) ));
		else if (boost::iequals(it->first, "album"))
			items.insert( std::make_pair(MetaData::Type::Album, stringTrim( stringToUTF8(it->second)) ));
		else if (boost::iequals(it->first, "title"))
			items.insert( std::make_pair(MetaData::Type::Title, stringTrim( stringToUTF8(it->second)) ));
		else if (boost::iequals(it->first, "track")) {
			std::size_t number;
			if (readAs<std::size_t>(it->second, number))
				items.insert( std::make_pair(MetaData::Type::TrackNumber, number ));
		}
		else if (boost::iequals(it->first, "disc"))
		{
			std::size_t number;
			if (readAs<std::size_t>(it->second, number))
				items.insert( std::make_pair(MetaData::Type::DiscNumber, number ));
		}
		else if (boost::iequals(it->first, "date")
				|| boost::iequals(it->first, "year")
				|| boost::iequals(it->first, "WM/Year"))
		{
			boost::posix_time::ptime p;
			if (readAsPosixTime(it->second, p))
				items.insert( std::make_pair(MetaData::Type::Date, p));
		}
		else if (boost::iequals(it->first, "TDOR")	// Original release time (ID3v2 2.4)
				|| boost::iequals(it->first, "TORY"))	// Original release year
		{
			boost::posix_time::ptime p;
			if (readAsPosixTime(it->second, p))
				items.insert( std::make_pair(MetaData::Type::OriginalDate, p));
		}
		else if (boost::iequals(it->first, "genre"))
		{
			// TODO use splitStrings
			std::list<std::string> genres;
			if (readList(it->second, ";,", genres))
				items.insert( std::make_pair(MetaData::Type::Genres, genres));

		}
		else if (boost::iequals(it->first, "MusicBrainz Artist Id")
			|| boost::iequals(it->first, "MUSICBRAINZ_ARTISTID"))
		{
			items.insert( std::make_pair(MetaData::Type::MusicBrainzArtistID, stringTrim( stringToUTF8(it->second)) ));
		}
		else if (boost::iequals(it->first, "MusicBrainz Album Id")
			|| boost::iequals(it->first, "MUSICBRAINZ_ALBUMID"))
		{
			items.insert( std::make_pair(MetaData::Type::MusicBrainzAlbumID, stringTrim( stringToUTF8(it->second)) ));
		}
	}

	return true;
}

} // namespace MetaData

