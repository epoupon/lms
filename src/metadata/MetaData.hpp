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

#ifndef METADATA_HPP
#define METADATA_HPP

#include <map>

#include <boost/any.hpp>
#include <boost/filesystem.hpp>

namespace MetaData
{

	enum class Type
	{
		Artist,			// string
		Title,			// string
		Album,			// string
		Genres,			// list<string>
		Duration,		// boost::posix_time::time_duration
		TrackNumber,		// size_t
		DiscNumber,		// size_t
		Date,			// boost::posix_time::ptime
		OriginalDate,		// boost::posix_time::ptime
		HasCover,		// bool
		AudioStreams,		// vector<AudioStream>
		VideoStreams,		// vector<VideoStream>
		SubtitleStreams,	// vector<SubtitleStream>
		MusicBrainzArtistID,	// string
		MusicBrainzAlbumID,	// string
	};

	// Used by Streams
	struct AudioStream {
		std::size_t nbChannels;
		std::size_t bitRate;
	};

	struct VideoStream {
		std::size_t bitRate;
	};

	struct SubtitleStream {
		;
	};

	// Type and associated data
	// See enum Type's comments
	typedef std::map<Type, boost::any>  Items;

	class Parser
	{
		public:

			typedef std::shared_ptr<Parser> pointer;

			virtual bool parse(const boost::filesystem::path& p, Items& items) = 0;

	};

} // namespace MetaData

#endif

