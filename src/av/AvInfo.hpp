/*
 * Copyright (C) 2015 Emeric Poupon
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

/* This file contains some classes in order to get info from file using the libavconv */

#pragma once

extern "C"
{
#define __STDC_CONSTANT_MACROS
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
}

#include <vector>
#include <string>
#include <cstdint>
#include <map>

#include <boost/optional.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp> //no i/o just types

#include "utils/Exception.hpp"

namespace Av
{

void AvInit();

struct Picture
{
	std::string mimeType;
	std::vector<uint8_t> data;
};

struct StreamInfo
{
	size_t		id;
	std::size_t     bitrate;
};

class MediaFileException : public LmsException
{
	public:
		MediaFileException(int avError);
};

class MediaFile
{
	public:
		MediaFile(const boost::filesystem::path& p);
		~MediaFile();

		// non copyable
		MediaFile(const MediaFile&) = delete;
		MediaFile& operator=(const MediaFile&) = delete;

		const boost::filesystem::path&		getPath() const {return _p;};

		boost::posix_time::time_duration	getDuration() const;
		std::map<std::string, std::string>	getMetaData(void);

		std::vector<StreamInfo>	getStreamInfo() const;
		boost::optional<std::size_t>	getBestStream() const; // none if failure/unknown
		bool			hasAttachedPictures(void) const;
		std::vector<Picture>	getAttachedPictures(std::size_t nbMaxPictures) const;

	private:
		MediaFile();

		boost::filesystem::path	_p;
		AVFormatContext* _context;
};

} // namespace Av

