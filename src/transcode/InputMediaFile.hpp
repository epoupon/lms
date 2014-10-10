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

#ifndef TRANSCODE_INPUT_MEDIA_FILE
#define TRANSCODE_INPUT_MEDIA_FILE

#include <map>
#include <string>

#include <boost/filesystem.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "cover/CoverArt.hpp"

#include "Stream.hpp"


namespace Transcode
{



class InputMediaFile
{
	public:

		enum StreamType
		{
			StreamAudio,
			StreamVideo,
			StreamSubtitle,
		};

		InputMediaFile(const boost::filesystem::path& p);

		// Accessors
		boost::filesystem::path			getPath(void) const		{return _path;}
		boost::posix_time::time_duration	getDuration(void) const		{return _duration;}

		// Pictures
		const std::vector< CoverArt::CoverArt >&	getCovers(void) const { return _covers; }

		// Stream handling
		const Stream&				getStream(Stream::Id id) const;
		std::vector<Stream>			getStreams(Stream::Type type) const;
		const std::map<Stream::Type, Stream::Id>& getBestStreams(void) const	{return _bestStreams;}

	private:

		boost::filesystem::path			_path;
		boost::posix_time::time_duration	_duration;

		std::vector<Stream>			_streams;
		std::map<Stream::Type, Stream::Id>	_bestStreams;

		std::vector< CoverArt::CoverArt > _covers;
};


} // namespace Transcode

#endif // TRANSCODE_INPUT_MEDIA_FILE

