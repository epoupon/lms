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

#include <gtest/gtest.h>

#include <array>
#include <span>
#include <string_view>

#include "FormatSignature.hpp"

namespace lms::image
{
    TEST(FormatSignature, jpeg)
    {
        constexpr std::array<std::byte, 8> data{ std::byte{ 0xFF }, std::byte{ 0xD8 }, std::byte{ 0xFF }, std::byte{ 0xE0 }, std::byte{ 0x00 }, std::byte{ 0x10 }, std::byte{ 0x4A }, std::byte{ 0x46 } };
        EXPECT_EQ(identifyFormat(data), core::media::ImageFormat::JPEG);
    }

    TEST(FormatSignature, png)
    {
        constexpr std::array<std::byte, 10> data{ std::byte{ 0x89 }, std::byte{ 0x50 }, std::byte{ 0x4E }, std::byte{ 0x47 }, std::byte{ 0x0D }, std::byte{ 0x0A }, std::byte{ 0x1A }, std::byte{ 0x0A }, std::byte{ 0x00 }, std::byte{ 0x00 } };
        EXPECT_EQ(identifyFormat(data), core::media::ImageFormat::PNG);
    }

    TEST(FormatSignature, bmp)
    {
        constexpr std::array<std::byte, 6> data{ std::byte{ 0x42 }, std::byte{ 0x4D }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 } };
        EXPECT_EQ(identifyFormat(data), core::media::ImageFormat::BMP);
    }

    TEST(FormatSignature, gif87a)
    {
        constexpr std::string_view str{ "GIF87a..." };
        EXPECT_EQ(identifyFormat(std::as_bytes(std::span{ str })), core::media::ImageFormat::GIF);
    }

    TEST(FormatSignature, gif89a)
    {
        constexpr std::string_view str{ "GIF89a..." };
        EXPECT_EQ(identifyFormat(std::as_bytes(std::span{ str })), core::media::ImageFormat::GIF);
    }

    TEST(FormatSignature, webp)
    {
        constexpr std::array<std::byte, 16> data{
            std::byte{ 'R' }, std::byte{ 'I' }, std::byte{ 'F' }, std::byte{ 'F' },
            std::byte{ 0x24 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 },
            std::byte{ 'W' }, std::byte{ 'E' }, std::byte{ 'B' }, std::byte{ 'P' },
            std::byte{ 'V' }, std::byte{ 'P' }, std::byte{ '8' }, std::byte{ ' ' }
        };
        EXPECT_EQ(identifyFormat(data), core::media::ImageFormat::WebP);
    }

    TEST(FormatSignature, riffButNotWebp)
    {
        constexpr std::array<std::byte, 16> data{
            std::byte{ 'R' }, std::byte{ 'I' }, std::byte{ 'F' }, std::byte{ 'F' },
            std::byte{ 0x24 }, std::byte{ 0x00 }, std::byte{ 0x00 }, std::byte{ 0x00 },
            std::byte{ 'W' }, std::byte{ 'A' }, std::byte{ 'V' }, std::byte{ 'E' },
            std::byte{ 'f' }, std::byte{ 'm' }, std::byte{ 't' }, std::byte{ ' ' }
        };
        EXPECT_EQ(identifyFormat(data), std::nullopt);
    }

    TEST(FormatSignature, svgNeverReturned)
    {
        constexpr std::string_view str{ "<?xml version=\"1.0\"?><svg></svg>" };
        EXPECT_EQ(identifyFormat(std::as_bytes(std::span{ str })), std::nullopt);
    }

    TEST(FormatSignature, emptyBuffer)
    {
        constexpr std::array<std::byte, 0> data{};
        EXPECT_EQ(identifyFormat(data), std::nullopt);
    }

    TEST(FormatSignature, shortBuffer)
    {
        constexpr std::array<std::byte, 2> data{ std::byte{ 0xFF }, std::byte{ 0xD8 } }; // truncated JPEG signature
        EXPECT_EQ(identifyFormat(data), std::nullopt);
    }

    TEST(FormatSignature, unrecognisedBytes)
    {
        constexpr std::array<std::byte, 8> data{ std::byte{ 0x00 }, std::byte{ 0x01 }, std::byte{ 0x02 }, std::byte{ 0x03 }, std::byte{ 0x04 }, std::byte{ 0x05 }, std::byte{ 0x06 }, std::byte{ 0x07 } };
        EXPECT_EQ(identifyFormat(data), std::nullopt);
    }
} // namespace lms::image
