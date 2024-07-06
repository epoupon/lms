/*
 * Copyright (C) 2024 Emeric Poupon
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
#include "core/ITraceLogger.hpp"

namespace lms::image
{
    std::unique_ptr<IRawImage> decodeImage(const std::byte* encodedData, std::size_t encodedDataSize)
    {
        LMS_SCOPED_TRACE_DETAILED("Image", "DecodeBuffer");
        return std::make_unique<STB::RawImage>(encodedData, encodedDataSize);
    }

    std::unique_ptr<IRawImage> decodeImage(const std::filesystem::path& path)
    {
        LMS_SCOPED_TRACE_DETAILED("Image", "DecodeFile");
        return std::make_unique<STB::RawImage>(path);
    }

    void init(const std::filesystem::path&)
    {
    }

    std::span<const std::filesystem::path> getSupportedFileExtensions()
    {
        static const std::array<std::filesystem::path, 4> fileExtensions{ ".jpg", ".jpeg", ".png", ".bmp" };
        return fileExtensions;
    }
} // namespace lms::image