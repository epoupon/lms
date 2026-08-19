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

#include "Backend.hpp"

#include <memory>

#include <Magick++/Image.h>

#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"
#include "image/Exception.hpp"

#include "EncodedImage.hpp"
#include "RawImage.hpp"

namespace lms::image::backend
{
    void init(const std::filesystem::path& path)
    {
        Magick::InitializeMagick(path.c_str());

        if (auto nbThreads{ MagickLib::GetMagickResourceLimit(MagickLib::ThreadsResource) }; nbThreads != 1)
            LMS_LOG(COVER, WARNING, "Consider setting env var OMP_NUM_THREADS=1 to save resources");

        if (!MagickLib::SetMagickResourceLimit(MagickLib::ThreadsResource, 1))
            LMS_LOG(COVER, ERROR, "Cannot set Magick thread resource limit to 1!");

        if (!MagickLib::SetMagickResourceLimit(MagickLib::DiskResource, 0))
            LMS_LOG(COVER, ERROR, "Cannot set Magick disk resource limit to 0!");

        LMS_LOG(COVER, INFO, "Magick threads resource limit = " << GetMagickResourceLimit(MagickLib::ThreadsResource));
        LMS_LOG(COVER, INFO, "Magick Disk resource limit = " << GetMagickResourceLimit(MagickLib::DiskResource));
    }

    ImageDimensions probeImage(const std::filesystem::path& path)
    {
        LMS_SCOPED_TRACE_DETAILED("Image", "ProbeFile");

        try
        {
            Magick::Image image;
            image.ping(path.c_str());

            ImageDimensions dimensions;
            dimensions.width = image.size().width();
            dimensions.height = image.size().height();

            return dimensions;
        }
        catch (Magick::Exception& e)
        {
            LMS_LOG(COVER, ERROR, "Caught Magick exception: " << e.what());
            throw Exception{ std::string{ "Magick probe error: " } + e.what() };
        }
    }

    ImageDimensions probeImage(std::span<const std::byte> encodedData)
    {
        LMS_SCOPED_TRACE_DETAILED("Image", "ProbeBuffer");

        try
        {
            Magick::Image image;
            Magick::Blob blob{ encodedData.data(), encodedData.size() };
            image.ping(blob);

            ImageDimensions dimensions;
            dimensions.width = image.size().width();
            dimensions.height = image.size().height();

            return dimensions;
        }
        catch (Magick::Exception& e)
        {
            LMS_LOG(COVER, ERROR, "Caught Magick exception: " << e.what());
            throw Exception{ std::string{ "Magick probe error: " } + e.what() };
        }
    }

    std::unique_ptr<IRawImage> decodeImage(std::span<const std::byte> encodedData)
    {
        LMS_SCOPED_TRACE_DETAILED("Image", "DecodeBuffer");
        return std::make_unique<GraphicsMagick::RawImage>(encodedData);
    }

    std::unique_ptr<IRawImage> decodeImage(const std::filesystem::path& path)
    {
        LMS_SCOPED_TRACE_DETAILED("Image", "DecodeFile");
        return std::make_unique<GraphicsMagick::RawImage>(path);
    }

    std::unique_ptr<IEncodedImage> encodeToJPEG(const IRawImage& rawImage, unsigned quality)
    {
        LMS_SCOPED_TRACE_DETAILED("Image", "WriteJPEG");

        try
        {
            Magick::Image image{ static_cast<const GraphicsMagick::RawImage&>(rawImage).getMagickImage() };
            image.magick("JPEG");
            image.quality(quality);

            Magick::Blob blob;
            image.write(&blob);

            return std::make_unique<EncodedImage>(std::span{ static_cast<const std::byte*>(blob.data()), blob.length() }, core::media::ImageFormat::JPEG);
        }
        catch (Magick::Exception& e)
        {
            LMS_LOG(COVER, ERROR, "Caught Magick exception: " << e.what());
            throw Exception{ std::string{ "Magick read error: " } + e.what() };
        }
    }

} // namespace lms::image::backend
