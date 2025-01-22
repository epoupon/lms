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

#include "EncodedImage.hpp"

#include <filesystem>
#include <fstream>
#include <unordered_map>

#include "core/ITraceLogger.hpp"
#include "core/String.hpp"
#include "image/Exception.hpp"

namespace lms::image
{
    namespace
    {
        std::string_view extensionToMimeType(const std::filesystem::path& extension)
        {
            static const std::unordered_map<std::string_view, std::string_view> mimeTypesByExtension{
                { ".bmp", "image/bmp" },
                { ".gif", "image/gif" },
                { ".jpeg", "image/jpeg" },
                { ".jpg", "image/jpeg" },
                { ".png", "image/png" },
                { ".ppm", "image/x-portable-pixmap" },
                { ".svg", "image/svg+xml" },
            };

            const auto it{ mimeTypesByExtension.find(core::stringUtils::stringToLower(extension.c_str())) };
            if (it == std::cend(mimeTypesByExtension))
                throw Exception{ "Unhandled image extension '" + extension.string() + "'" };
            return it->second;
        }

        std::vector<std::byte> fileToBuffer(const std::filesystem::path& p)
        {
            LMS_SCOPED_TRACE_DETAILED("Image", "ReadFile");

            std::ifstream ifs{ p, std::ios::binary };
            if (!ifs.is_open())
                throw Exception{ "Cannot open file '" + p.string() + "' for reading purpose" };

            std::vector<std::byte> data;
            // read file content
            ifs.seekg(0, std::ios::end);
            std::streamsize size = ifs.tellg();
            if (size < 0)
                throw Exception{ "Cannot determine file size for '" + p.string() + "'" };

            ifs.seekg(0, std::ios::beg);
            data.resize(size);
            if (!ifs.read(reinterpret_cast<char*>(data.data()), size))
                throw Exception{ "Cannot read file content for '" + p.string() + "'" };

            return data;
        }

    } // namespace

    std::unique_ptr<IEncodedImage> readImage(const std::filesystem::path& path)
    {
        return std::make_unique<EncodedImage>(path);
    }

    std::unique_ptr<IEncodedImage> readImage(std::span<const std::byte> encodedData, std::string_view mimeType)
    {
        return std::make_unique<EncodedImage>(encodedData, mimeType);
    }

    EncodedImage::EncodedImage(std::vector<std::byte>&& data, std::string_view mimeType)
        : _data{ std::move(data) }
        , _mimeType(mimeType)
    {
    }

    EncodedImage::EncodedImage(std::span<const std::byte> data, std::string_view mimeType)
        : _data(std::cbegin(data), std::cend(data))
        , _mimeType(mimeType)
    {
    }

    EncodedImage::EncodedImage(const std::filesystem::path& p)
        : EncodedImage::EncodedImage{ fileToBuffer(p), extensionToMimeType(p.extension()) }
    {
    }
} // namespace lms::image