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

#include "Image.hpp"

#include "utils/Logger.hpp"

namespace CoverArt {

void
init(const std::filesystem::path& path)
{
	Magick::InitializeMagick(path.string().c_str());
}

static
std::string
formatToMagick(Format format)
{
	switch (format)
	{
		case Format::JPEG: return "JPEG";
	}

	return "JPEG";
}

std::string
formatToMimeType(Format format)
{
	switch (format)
	{
		case Format::JPEG: return "JPEG";
	}

	return "application/octet-stream";
}


bool
Image::load(const std::vector<unsigned char>& rawData)
{
	try
	{
		Magick::Blob blob {&rawData[0], rawData.size()};
		_image.read(blob);

		return true;
	}
	catch (Magick::Exception& e)
	{
		LMS_LOG(COVER, ERROR) << "Caught Magick exception while loading raw image: " << e.what();
		return false;
	}
}

bool
Image::load(const std::filesystem::path& p)
{
	try
	{
		_image.read(p.string());

		return true;
	}
	catch (Magick::Exception& e)
	{
		LMS_LOG(COVER, ERROR) << "Caught Magick exception while loading image from file '" << p.string() << "': " << e.what();
		return false;
	}
}

Geometry
Image::getSize() const
{
	Magick::Geometry geometry {_image.size()};
	return {geometry.width(), geometry.height()};
}

bool
Image::scale(Geometry geometry)
{
	if (geometry.width == 0 || geometry.height == 0)
		return false;

	try
	{
		_image.resize( Magick::Geometry(geometry.width, geometry.height ) );

		return true;
	}
	catch (Magick::Exception& e)
	{
		LMS_LOG(COVER, ERROR) << "Caught Magick exception during scale: " << e.what();
		return false;
	}
}

std::vector<uint8_t>
Image::save(Format format) const
{
	std::vector<uint8_t> res;

	try
	{
		Magick::Image outputImage {_image};

		outputImage.magick(formatToMagick(format));

		Magick::Blob blob;
		outputImage.write(&blob);

		auto begin = static_cast<const uint8_t*>(blob.data());
		std::copy(begin, begin + blob.length(), std::back_inserter(res));
		return res;
	}
	catch (Magick::Exception& e)
	{
		LMS_LOG(COVER, ERROR) << "Caught Magick exception during save:" << e.what();
		res.clear();
		return res;
	}
}

} // namespace CoverArt

