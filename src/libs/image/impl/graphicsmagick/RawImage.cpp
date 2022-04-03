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

#include "RawImage.hpp"

#include <magick/resource.h>

#include "JPEGImage.hpp"
#include "image/Exception.hpp"
#include "utils/Logger.hpp"

namespace Image
{
	std::unique_ptr<IRawImage> decodeImage(const std::byte* encodedData, std::size_t encodedDataSize)
	{
		return std::make_unique<GraphicsMagick::RawImage>(encodedData, encodedDataSize);
	}

	std::unique_ptr<IRawImage> decodeImage(const std::filesystem::path& path)
	{
		return std::make_unique<GraphicsMagick::RawImage>(path);
	}

	void
	init(const std::filesystem::path& path)
	{
		Magick::InitializeMagick(path.string().c_str());

		if (auto nbThreads {MagickLib::GetMagickResourceLimit(MagickLib::ThreadsResource)}; nbThreads != 1)
			LMS_LOG(COVER, WARNING) << "Consider setting env var OMP_NUM_THREADS=1 to save resources";

		if (!MagickLib::SetMagickResourceLimit(MagickLib::ThreadsResource, 1))
			LMS_LOG(COVER, ERROR) << "Cannot set Magick thread resource limit to 1!";

		if (!MagickLib::SetMagickResourceLimit(MagickLib::DiskResource, 0))
			LMS_LOG(COVER, ERROR) << "Cannot set Magick disk resource limit to 0!";

		LMS_LOG(COVER, INFO) << "Magick threads resource limit = " << GetMagickResourceLimit(MagickLib::ThreadsResource);
		LMS_LOG(COVER, INFO) << "Magick Disk resource limit = " << GetMagickResourceLimit(MagickLib::DiskResource);
	}
}

namespace Image::GraphicsMagick
{

RawImage::RawImage(const std::byte* encodedData, std::size_t encodedDataSize)
{
	try
	{
		Magick::Blob blob {encodedData, encodedDataSize};
		_image.read(blob);
	}
	catch (Magick::WarningCoder& e)
	{
		LMS_LOG(COVER, WARNING) << "Caught Magick WarningCoder: " << e.what();
	}
	catch (Magick::Warning& e)
	{
		LMS_LOG(COVER, WARNING) << "Caught Magick warning: " << e.what();
		throw ImageException {std::string {"Magick read warning: "} + e.what()};
	}
	catch (Magick::Exception& e)
	{
		LMS_LOG(COVER, ERROR) << "Caught Magick exception: " << e.what();
		throw ImageException {std::string {"Magick read error: "} + e.what()};
	}
}

RawImage::RawImage(const std::filesystem::path& p)
{
	try
	{
		_image.read(p.string().c_str());
	}
	catch (Magick::WarningCoder& e)
	{
		LMS_LOG(COVER, WARNING) << "Caught Magick WarningCoder: " << e.what();
	}
	catch (Magick::Warning& e)
	{
		LMS_LOG(COVER, WARNING) << "Caught Magick warning: " << e.what();
		throw ImageException {std::string {"Magick read warning: "} + e.what()};
	}
	catch (Magick::Exception& e)
	{
		LMS_LOG(COVER, ERROR) << "Caught Magick exception: " << e.what();
		throw ImageException {std::string {"Magick read error: "} + e.what()};
	}
}

void
RawImage::resize(ImageSize width)
{
	try
	{
		_image.resize(Magick::Geometry {static_cast<unsigned int>(width), static_cast<unsigned int>(width)});
	}
	catch (Magick::Exception& e)
	{
		LMS_LOG(COVER, ERROR) << "Caught Magick exception while resizing: " << e.what();
		throw ImageException {std::string {"Magick resize error: "} + e.what()};
	}
}

std::unique_ptr<IEncodedImage>
RawImage::encodeToJPEG(unsigned quality) const
{
	return std::make_unique<JPEGImage>(*this, quality);
}

Magick::Image
RawImage::getMagickImage() const
{
	return _image;
}

} // namespace Image::GraphicsMagick

