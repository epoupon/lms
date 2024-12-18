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

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "core/ITraceLogger.hpp"

#include "EncodedImage.hpp"
#include "RawImage.hpp"
#include "image/Exception.hpp"

namespace lms::image
{
    void init(const std::filesystem::path& /*unused*/)
    {
    }

    std::span<const std::filesystem::path> getSupportedFileExtensions()
    {
        static const std::array<std::filesystem::path, 4> fileExtensions{ ".jpg", ".jpeg", ".png", ".bmp" };
        return fileExtensions;
    }

    std::unique_ptr<IRawImage> decodeImage(std::span<const std::byte> encodedData)
    {
        LMS_SCOPED_TRACE_DETAILED("Image", "DecodeBuffer");
        return std::make_unique<STB::RawImage>(encodedData);
    }

    std::unique_ptr<IRawImage> decodeImage(const std::filesystem::path& path)
    {
        LMS_SCOPED_TRACE_DETAILED("Image", "DecodeFile");
        return std::make_unique<STB::RawImage>(path);
    }

    std::unique_ptr<IEncodedImage> encodeToJPEG(const IRawImage& rawImage, unsigned quality)
    {
        LMS_SCOPED_TRACE_DETAILED("Image", "WriteJPEG");

        std::vector<std::byte> encodedData;

        auto writeCb{ [](void* ctx, void* writeData, int writeSize) {
            auto& output{ *reinterpret_cast<std::vector<std::byte>*>(ctx) };
            const std::size_t currentOutputSize{ output.size() };
            output.resize(currentOutputSize + writeSize);
            std::copy(reinterpret_cast<const std::byte*>(writeData), reinterpret_cast<const std::byte*>(writeData) + writeSize, output.data() + currentOutputSize);
        } };

        if (::stbi_write_jpg_to_func(writeCb, &encodedData, rawImage.getWidth(), rawImage.getHeight(), 3, static_cast<const STB::RawImage&>(rawImage).getData(), quality) == 0)
            throw Exception{ "Failed to export in jpeg format!" };

        return std::make_unique<EncodedImage>(std::move(encodedData), "image/jpeg");
    }
} // namespace lms::image