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

#include <atomic>
#include <fstream>

#include <magick/resource.h>

#include "utils/Logger.hpp"

namespace CoverArt {

void
init(const std::filesystem::path& path)
{
	Magick::InitializeMagick(path.string().c_str());

	if (!MagickLib::SetMagickResourceLimit(MagickLib::ThreadsResource, 1))
		LMS_LOG(COVER, ERROR) << "Cannot set Magick thread resource limit to 1!";

	if (!MagickLib::SetMagickResourceLimit(MagickLib::DiskResource, 0))
		LMS_LOG(COVER, ERROR) << "Cannot set Magick disk resource limit to 0!";

	LMS_LOG(COVER, INFO) << "Magick threads resource limit = " << GetMagickResourceLimit(MagickLib::ThreadsResource);
	LMS_LOG(COVER, INFO) << "Magick Disk resource limit = " << GetMagickResourceLimit(MagickLib::DiskResource);
}

EncodedImage::EncodedImage(const std::byte* data, std::size_t dataSize)
: _blob {data, dataSize}
{
}

EncodedImage::EncodedImage(Magick::Blob blob)
:  _blob {blob}
{
}

const std::byte*
EncodedImage::getData() const
{
	return reinterpret_cast<const std::byte*>(_blob.data());
}

std::size_t
EncodedImage::getDataSize() const
{
	return _blob.length();
}

RawImage::RawImage(const std::filesystem::path& p)
{
	try
	{
		_image.read(p.string().c_str());
	}
	catch (Magick::WarningCoder& e)
	{
		LMS_LOG(COVER, WARNING) << "Caught Magick WarningCoder while loading image '" << p.string() << "': " << e.what();
	}
	catch (Magick::Warning& e)
	{
		LMS_LOG(COVER, WARNING) << "Caught Magick warning while loading raw image '" << p.string() << "': " << e.what();
		throw ImageException {std::string {"Magick read warning: "} + e.what()};
	}
	catch (Magick::Exception& e)
	{
		LMS_LOG(COVER, ERROR) << "Caught Magick exception while loading raw image '" << p.string() << "': " << e.what();
		throw ImageException {std::string {"Magick read error: "} + e.what()};
	}
}

RawImage::RawImage(const EncodedImage& encodedImage)
{
	try
	{
		_image.read(encodedImage._blob);
	}
	catch (Magick::WarningCoder& e)
	{
		LMS_LOG(COVER, WARNING) << "Caught Magick WarningCoder while loading raw image: " << e.what();
	}
	catch (Magick::Warning& e)
	{
		LMS_LOG(COVER, WARNING) << "Caught Magick warning while loading raw image: " << e.what();
		throw ImageException {std::string {"Magick read warning: "} + e.what()};
	}
	catch (Magick::Exception& e)
	{
		LMS_LOG(COVER, ERROR) << "Caught Magick exception while loading raw image: " << e.what();
		throw ImageException {std::string {"Magick read error: "} + e.what()};
	}
}

void
RawImage::scale(std::size_t width)
{
	if (width == 0)
		throw ImageException {"Bad width = 0"};

	try
	{
		_image.resize(Magick::Geometry {static_cast<unsigned int>(width), static_cast<unsigned int>(width)});
	}
	catch (Magick::Exception& e)
	{
		LMS_LOG(COVER, ERROR) << "Caught Magick exception during scale: " << e.what();
		throw ImageException {std::string {"Magick resize error: "} + e.what()};
	}
}

EncodedImage
RawImage::encode() const
{
	try
	{
		Magick::Image outputImage {_image};

		outputImage.magick("JPEG");

		Magick::Blob blob;
		outputImage.write(&blob);

		return EncodedImage {blob};
	}
	catch (Magick::Exception& e)
	{
		LMS_LOG(COVER, ERROR) << "Caught Magick exception while encoding raw image: " << e.what();
		throw ImageException {std::string {"Magick encode error: "} + e.what()};
	}
}

} // namespace CoverArt

