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

#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "av/InputFormatContext.hpp"

#include "logger/Logger.hpp"

#include "Utils.hpp"

namespace MetaData
{

void
AvFormat::parse(const boost::filesystem::path& p, Items& items)
{

	try {

		Av::InputFormatContext input(p);
		input.findStreamInfo(); // needed by input.getDurationSecs

		std::map<std::string, std::string> metadata;
		input.getMetadata().get(metadata);

		// HACK or OGG files
		// If we did not find tags, searched metadata in streams
		if (metadata.empty())
		{
			// Get input streams
			std::vector<Av::Stream> streams = input.getStreams();

			BOOST_FOREACH(Av::Stream& stream, streams)
			{
				stream.getMetadata().get(metadata);

				if (!metadata.empty())
					break;
			}
		}


		// Stream info
		{
			std::vector<Av::Stream> avStreams = input.getStreams();

			std::vector<AudioStream>	audioStreams;
			std::vector<VideoStream>	videoStreams;
			std::vector<SubtitleStream>	subtitleStreams;

			BOOST_FOREACH(Av::Stream& avStream, avStreams)
			{
				switch(avStream.getCodecContext().getType())
				{
					case AVMEDIA_TYPE_VIDEO:
						if (!avStream.hasAttachedPic())
						{
							VideoStream stream;
							stream.bitRate = avStream.getCodecContext().getBitRate();
							videoStreams.push_back(stream);
						}
						break;

					case AVMEDIA_TYPE_AUDIO:
						{
							AudioStream stream;
							stream.nbChannels = avStream.getCodecContext().getNbChannels();
							stream.bitRate = avStream.getCodecContext().getBitRate();
							audioStreams.push_back(stream);
						}
						break;

					case AVMEDIA_TYPE_SUBTITLE:
						{
							subtitleStreams.push_back( SubtitleStream() );
						}
						break;

					default:
						break;
				}
			}

			if (!videoStreams.empty())
				items.insert( std::make_pair(MetaData::Type::VideoStreams, videoStreams));
			if (!audioStreams.empty())
				items.insert( std::make_pair(MetaData::Type::AudioStreams, audioStreams));
			if (!subtitleStreams.empty())
				items.insert( std::make_pair(MetaData::Type::SubtitleStreams, subtitleStreams));

		}

		// Duration
		items.insert( std::make_pair(MetaData::Type::Duration, boost::posix_time::time_duration( boost::posix_time::seconds( input.getDurationSecs() )) ));

		// Embedded MetaData
		// Make sure to convert strings into UTF-8
		std::map<std::string, std::string>::const_iterator it;
		for (it = metadata.begin(); it != metadata.end(); ++it)
		{
			if (boost::iequals(it->first, "artist"))
				items.insert( std::make_pair(MetaData::Type::Artist, string_trim( string_to_utf8(it->second)) ));
			else if (boost::iequals(it->first, "album"))
				items.insert( std::make_pair(MetaData::Type::Album, string_trim( string_to_utf8(it->second)) ));
			else if (boost::iequals(it->first, "title"))
				items.insert( std::make_pair(MetaData::Type::Title, string_trim( string_to_utf8(it->second)) ));
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
				std::list<std::string> genres;
				if (readList(it->second, ";,", genres))
					items.insert( std::make_pair(MetaData::Type::Genres, genres));

			}
/*			else
				LMS_LOG(MOD_METADATA, SEV_DEBUG) << "key = " << it->first << ", value = " << it->second;
*/
		}

	}
	catch(std::exception &e)
	{
		LMS_LOG(MOD_METADATA, SEV_ERROR) << "Parsing of '" << p << "' failed!";
	}

}

} // namespace MetaData

