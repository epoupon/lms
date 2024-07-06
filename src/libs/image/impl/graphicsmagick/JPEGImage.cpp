/*
 * Copyright (C) 2020 Emeric Poupon
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

#include "JPEGImage.hpp"

#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"
#include "image/Exception.hpp"

#include "RawImage.hpp"

namespace lms::image::GraphicsMagick
{
    JPEGImage::JPEGImage(const RawImage& rawImage, unsigned quality)
    {
        LMS_SCOPED_TRACE_DETAILED("Image", "WriteJPEG");

        try
        {
            Magick::Image image{ rawImage.getMagickImage() };
            image.magick("JPEG");
            image.quality(quality);
            image.write(&_blob);
        }
        catch (Magick::Exception& e)
        {
            LMS_LOG(COVER, ERROR, "Caught Magick exception: " << e.what());
            throw Exception{ std::string{ "Magick read error: " } + e.what() };
        }
    }

    const std::byte* JPEGImage::getData() const
    {
        return reinterpret_cast<const std::byte*>(_blob.data());
    }

    std::size_t JPEGImage::getDataSize() const
    {
        return _blob.length();
    }
} // namespace lms::image::GraphicsMagick
