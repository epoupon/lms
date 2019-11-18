/*
 * Copyright (C) 2019 Emeric Poupon
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

#include "AvTypes.hpp"

#include <map>

namespace Av {

const char* encodingToMimetype(Encoding encoding)
{
	switch (encoding)
	{
		case Encoding::MP3: 		return "audio/mpeg";
		case Encoding::OGG_OPUS:	return "audio/opus";
		case Encoding::MATROSKA_OPUS:	return "audio/x-matroska";
		case Encoding::OGG_VORBIS:	return "audio/ogg";
		case Encoding::WEBM_VORBIS:	return "audio/webm";
	}

	throw AvException("Invalid encoding");
}

}

