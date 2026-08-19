/*
 * Copyright (C) 2026 Emeric Poupon
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

#include "FormatSignature.hpp"

#include <algorithm>
#include <array>

namespace lms::image
{
    namespace
    {
        bool startsWith(std::span<const std::byte> header, std::span<const std::byte> signature, std::size_t offset = 0)
        {
            if (header.size() < offset + signature.size())
                return false;

            return std::equal(std::cbegin(signature), std::cend(signature), std::cbegin(header) + offset);
        }

        bool isJPEG(std::span<const std::byte> header)
        {
            constexpr std::array signature{ std::byte{ 0xFF }, std::byte{ 0xD8 }, std::byte{ 0xFF } };
            return startsWith(header, signature);
        }

        bool isPNG(std::span<const std::byte> header)
        {
            constexpr std::array signature{ std::byte{ 0x89 }, std::byte{ 0x50 }, std::byte{ 0x4E }, std::byte{ 0x47 }, std::byte{ 0x0D }, std::byte{ 0x0A }, std::byte{ 0x1A }, std::byte{ 0x0A } };
            return startsWith(header, signature);
        }

        bool isBMP(std::span<const std::byte> header)
        {
            constexpr std::array signature{ std::byte{ 0x42 }, std::byte{ 0x4D } }; // "BM"
            return startsWith(header, signature);
        }

        bool isGIF(std::span<const std::byte> header)
        {
            constexpr std::array signature87{ std::byte{ 'G' }, std::byte{ 'I' }, std::byte{ 'F' }, std::byte{ '8' }, std::byte{ '7' }, std::byte{ 'a' } };
            constexpr std::array signature89{ std::byte{ 'G' }, std::byte{ 'I' }, std::byte{ 'F' }, std::byte{ '8' }, std::byte{ '9' }, std::byte{ 'a' } };
            return startsWith(header, signature87) || startsWith(header, signature89);
        }

        bool isWebP(std::span<const std::byte> header)
        {
            static constexpr std::array riff{ std::byte{ 'R' }, std::byte{ 'I' }, std::byte{ 'F' }, std::byte{ 'F' } };
            static constexpr std::array webp{ std::byte{ 'W' }, std::byte{ 'E' }, std::byte{ 'B' }, std::byte{ 'P' } };
            return startsWith(header, riff) && startsWith(header, webp, 8);
        }
    } // namespace

    std::optional<core::media::ImageFormat> identifyFormat(std::span<const std::byte> header)
    {
        if (isJPEG(header))
            return core::media::ImageFormat::JPEG;
        if (isPNG(header))
            return core::media::ImageFormat::PNG;
        if (isBMP(header))
            return core::media::ImageFormat::BMP;
        if (isGIF(header))
            return core::media::ImageFormat::GIF;
        if (isWebP(header))
            return core::media::ImageFormat::WebP;

        return std::nullopt;
    }
} // namespace lms::image
