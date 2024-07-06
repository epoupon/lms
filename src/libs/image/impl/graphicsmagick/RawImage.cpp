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

#include <algorithm>
#include <array>

#include <magick/resource.h>

#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"
#include "image/Exception.hpp"

#include "JPEGImage.hpp"

namespace lms::image::GraphicsMagick
{
    RawImage::RawImage(const std::byte* encodedData, std::size_t encodedDataSize)
    {
        try
        {
            Magick::Blob blob{ encodedData, encodedDataSize };
            _image.read(blob);
        }
        catch (Magick::WarningCoder& e)
        {
            LMS_LOG(COVER, WARNING, "Caught Magick WarningCoder: " << e.what());
        }
        catch (Magick::Warning& e)
        {
            LMS_LOG(COVER, WARNING, "Caught Magick warning: " << e.what());
            throw Exception{ std::string{ "Magick read warning: " } + e.what() };
        }
        catch (Magick::Exception& e)
        {
            LMS_LOG(COVER, ERROR, "Caught Magick exception: " << e.what());
            throw Exception{ std::string{ "Magick read error: " } + e.what() };
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
            LMS_LOG(COVER, WARNING, "Caught Magick WarningCoder: " << e.what());
        }
        catch (Magick::Warning& e)
        {
            LMS_LOG(COVER, WARNING, "Caught Magick warning: " << e.what());
            throw Exception{ std::string{ "Magick read warning: " } + e.what() };
        }
        catch (Magick::Exception& e)
        {
            LMS_LOG(COVER, ERROR, "Caught Magick exception: " << e.what());
            throw Exception{ std::string{ "Magick read error: " } + e.what() };
        }
    }

    ImageSize RawImage::getWidth() const
    {
        return _image.size().width();
    }

    ImageSize RawImage::getHeight() const
    {
        return _image.size().height();
    }

    void RawImage::resize(ImageSize width)
    {
        try
        {
            LMS_SCOPED_TRACE_DETAILED("Image", "Resize");

            _image.resize(Magick::Geometry{ static_cast<unsigned int>(width), static_cast<unsigned int>(width) });
        }
        catch (Magick::Exception& e)
        {
            LMS_LOG(COVER, ERROR, "Caught Magick exception while resizing: " << e.what());
            throw Exception{ std::string{ "Magick resize error: " } + e.what() };
        }
    }

    std::unique_ptr<IEncodedImage> RawImage::encodeToJPEG(unsigned quality) const
    {
        return std::make_unique<JPEGImage>(*this, quality);
    }

    Magick::Image RawImage::getMagickImage() const
    {
        return _image;
    }

} // namespace lms::image::GraphicsMagick
