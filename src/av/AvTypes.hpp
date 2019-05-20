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

#pragma once

#include <string>
#include <boost/filesystem.hpp>

#include "utils/Exception.hpp"

namespace Av {

class AvException : public LmsException
{
	public:
		AvException(const std::string& msg) : LmsException(msg) {}
};

enum class Encoding
{
	// Values are important and must not be changed
	MP3		= 0,
	OGG_OPUS	= 1,
	OGG_VORBIS	= 2,
	WEBM_VORBIS	= 3,
};

std::string encodingToMimetype(Encoding encoding);

}

