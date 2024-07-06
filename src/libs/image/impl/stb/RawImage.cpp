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

#include "RawImage.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION

#define STBIR_DEFAULT_FILTER_DOWNSAMPLE STBIR_FILTER_MITCHELL
#define STBIR_DEFAULT_FILTER_UPSAMPLE STBIR_FILTER_CATMULLROM

#define STBI_FAILURE_USERMSG

#include <stb_image.h>
#if STB_IMAGE_RESIZE_VERSION == 1
    #include <stb_image_resize.h>
#elif STB_IMAGE_RESIZE_VERSION == 2
    #include <stb_image_resize2.h>
#else
    #error "Unhandled STB image resize version"!
#endif

#include "core/ITraceLogger.hpp"
#include "image/Exception.hpp"

#include "JPEGImage.hpp"

namespace lms::image::STB
{
    RawImage::RawImage(const std::byte* encodedData, std::size_t encodedDataSize)
    {
        int n;
        _data = UniquePtrFree{ ::stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(encodedData), encodedDataSize, &_width, &_height, &n, 3), std::free };
        if (!_data)
            throw Exception{ "Cannot load image from memory: " + std::string{ ::stbi_failure_reason() } };
    }

    RawImage::RawImage(const std::filesystem::path& p)
    {
        int n;
        _data = UniquePtrFree{ stbi_load(p.string().c_str(), &_width, &_height, &n, 3), std::free };
        if (!_data)
            throw Exception{ "Cannot load image from file: " + std::string{ ::stbi_failure_reason() } };
    }

    void RawImage::resize(ImageSize width)
    {
        LMS_SCOPED_TRACE_DETAILED("Image", "Resize");

        size_t height;
        if (_width == _height)
        {
            height = width;
        }
        else if (_width > _height)
        {
            height = (size_t)((float)width / _width * _height);
        }
        else
        {
            height = width;
            width = (size_t)((float)height / _height * _width);
        }

        UniquePtrFree resizedData{ reinterpret_cast<unsigned char*>(malloc(width * height * 3)), std::free };
        if (!resizedData)
            throw Exception{ "Cannot allocate memory for resized image!" };

#if STB_IMAGE_RESIZE_VERSION == 1
        if (::stbir_resize_uint8_srgb(reinterpret_cast<const unsigned char*>(_data.get()), _width, _height, 0,
                reinterpret_cast<unsigned char*>(resizedData.get()), width, height, 0,
                3, STBIR_ALPHA_CHANNEL_NONE, 0)
            == 0)
#elif STB_IMAGE_RESIZE_VERSION == 2
        if (::stbir_resize_uint8_srgb(reinterpret_cast<const unsigned char*>(_data.get()), _width, _height, 0,
                reinterpret_cast<unsigned char*>(resizedData.get()), width, height, 0,
                STBIR_RGB)
            == 0)
#else
    #error "Unhandled STB image resize version"!
#endif
        {
            throw Exception{ "Failed to resize image:" + std::string{ ::stbi_failure_reason() } };
        }

        _data = std::move(resizedData);
        _height = height;
        _width = width;
    }

    std::unique_ptr<IEncodedImage> RawImage::encodeToJPEG(unsigned quality) const
    {
        return std::make_unique<JPEGImage>(*this, quality);
    }

    ImageSize RawImage::getWidth() const
    {
        return _width;
    }

    ImageSize RawImage::getHeight() const
    {
        return _height;
    }

    const std::byte* RawImage::getData() const
    {
        if (!_data)
            return nullptr;

        return reinterpret_cast<const std::byte*>(_data.get());
    }
} // namespace lms::image::STB