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

std::string encodingToMimetype(Encoding encoding)
{
	static const std::map<Encoding, std::string> encodings
	{
		{Encoding::MP3,		"audio/mp3"},
			{Encoding::OGG_VORBIS,	"audio/ogg"},
			{Encoding::OGG_OPUS,	"audio/opus"},
			{Encoding::WEBM_VORBIS,	"audio/webm"},
	};

	auto it {encodings.find(encoding)};
	if (it == encodings.end())
		throw AvException("Invalid encoding");

	return it->second;
}

Encoding guessEncoding(const boost::filesystem::path& file)
{

}

}

