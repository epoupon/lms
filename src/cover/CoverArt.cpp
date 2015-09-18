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
#include "logger/Logger.hpp"

#include "CoverArt.hpp"


namespace CoverArt {

static
std::string format_to_magick(Format format)
{
	switch (format)
	{
		case Format::JPEG: return "JPEG";
	}

	return "JPEG";
}

std::string format_to_mimeType(Format format)
{
	switch (format)
	{
		case Format::JPEG: return "JPEG";
	}

	return "application/octet-stream";
}

void
CoverArt::init(const char *path)
{
	Magick::InitializeMagick(path);
}

CoverArt::CoverArt(const std::vector<unsigned char>& rawData)
{
	Magick::Blob blob(&rawData[0], rawData.size());
	_image.read(blob);
}


bool
CoverArt::scale(std::size_t size)
{
	if (!size)
		return false;

	try
	{
		_image.resize( Magick::Geometry(size, size ) );

		return true;
	}
	catch (Magick::Exception& e)
	{
		LMS_LOG(COVER, ERROR) << "Caught exception: " << e.what();
		return false;
	}
}

void
CoverArt::getData(std::vector<unsigned char>& data, Format format) const
{

	Magick::Image outputImage(_image);

	outputImage.magick( format_to_magick(format));

	Magick::Blob blob;
	outputImage.write(&blob);

	unsigned char *charBuf = (unsigned char*)blob.data();
	data.assign( charBuf, charBuf + blob.length() );
}

} // namespace CoverArt
