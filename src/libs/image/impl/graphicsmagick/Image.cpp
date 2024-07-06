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

#include "image/Image.hpp"

#include <array>

#include "RawImage.hpp"
#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"

namespace lms::image
{
    std::unique_ptr<IRawImage> decodeImage(const std::byte* encodedData, std::size_t encodedDataSize)
    {
        LMS_SCOPED_TRACE_DETAILED("Image", "DecodeBuffer");
        return std::make_unique<GraphicsMagick::RawImage>(encodedData, encodedDataSize);
    }

    std::unique_ptr<IRawImage> decodeImage(const std::filesystem::path& path)
    {
        LMS_SCOPED_TRACE_DETAILED("Image", "DecodeFile");
        return std::make_unique<GraphicsMagick::RawImage>(path);
    }

    void init(const std::filesystem::path& path)
    {
        Magick::InitializeMagick(path.string().c_str());

        if (auto nbThreads{ MagickLib::GetMagickResourceLimit(MagickLib::ThreadsResource) }; nbThreads != 1)
            LMS_LOG(COVER, WARNING, "Consider setting env var OMP_NUM_THREADS=1 to save resources");

        if (!MagickLib::SetMagickResourceLimit(MagickLib::ThreadsResource, 1))
            LMS_LOG(COVER, ERROR, "Cannot set Magick thread resource limit to 1!");

        if (!MagickLib::SetMagickResourceLimit(MagickLib::DiskResource, 0))
            LMS_LOG(COVER, ERROR, "Cannot set Magick disk resource limit to 0!");

        LMS_LOG(COVER, INFO, "Magick threads resource limit = " << GetMagickResourceLimit(MagickLib::ThreadsResource));
        LMS_LOG(COVER, INFO, "Magick Disk resource limit = " << GetMagickResourceLimit(MagickLib::DiskResource));
    }

    std::span<const std::filesystem::path> getSupportedFileExtensions()
    {
        static const std::array<std::filesystem::path, 4> fileExtensions{ ".jpg", ".jpeg", ".png", ".bmp" };
        return fileExtensions;
    }
} // namespace lms::image
