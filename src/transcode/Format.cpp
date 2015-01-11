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

#include <boost/foreach.hpp>
#include <stdexcept>
#include <cassert>

#include "Format.hpp"

namespace Transcode
{

const std::vector<Format> Format::_supportedFormats
{
	{Format::OGA, Format::Audio, "audio/ogg", "Ogg"},
	{Format::OGV, Format::Video, "video/ogg", "Ogg"},
	{Format::MP3, Format::Audio, "audio/mpeg", "MP3"},
	{Format::WEBMA, Format::Audio, "audio/webm", "WebM"},
	{Format::WEBMV, Format::Video, "video/webm", "WebM"},
	{Format::FLV, Format::Video, "video/x-flv", "Flash Video"},
	{Format::M4A, Format::Audio, "audio/mp4", "MP4"},
	{Format::M4V, Format::Video, "video/mp4", "MP4"},
};

Format::Format(Encoding encoding, Type type, std::string mimeType, std::string desc)
:
_encoding(encoding),
_type(type),
_mineType(mimeType),
_desc(desc)
{}


const Format&
Format::get(Encoding encoding)
{
	BOOST_FOREACH(const Format& format, _supportedFormats)
	{
		if (format.getEncoding() == encoding)
			return format;
	}

	throw std::runtime_error("Cannot find format");
}

std::vector<Format>
Format::get(Type type)
{
	std::vector<Format> res;
	BOOST_FOREACH(const Format& format, _supportedFormats)
	{
		if (format.getType() == type)
			res.push_back( format );
	}

	return res;
}

bool
Format::operator==(const Format& other) const
{
	return getEncoding() == other.getEncoding();
}


} // namespace Transcode

