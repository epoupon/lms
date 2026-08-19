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

#include "core/UUID.hpp"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>

#include "core/Random.hpp"

namespace lms::core
{
    namespace stringUtils
    {
        template<>
        std::optional<UUID> readAs(std::string_view str)
        {
            return UUID::fromString(str);
        }
    } // namespace stringUtils

    namespace
    {
        // Each entry is the str index of the high hex char for that UUID byte
        constexpr std::array<std::size_t, 16> byteOffsets{
            0, 2, 4, 6,            // group 1 (4 bytes, positions 0-7)
            9, 11,                 // group 2 (2 bytes, positions 9-12)
            14, 16,                // group 3 (2 bytes, positions 14-17)
            19, 21,                // group 4 (2 bytes, positions 19-22)
            24, 26, 28, 30, 32, 34 // group 5 (6 bytes, positions 24-35)
        };

        bool parseUUID(std::string_view str, std::array<std::byte, 16>& out)
        {
            if (str.size() != 36 || str[8] != '-' || str[13] != '-' || str[18] != '-' || str[23] != '-')
                return false;

            for (std::size_t i{}; i < 16; ++i)
            {
                unsigned int byte{};
                const char* begin{ str.data() + byteOffsets[i] };
                const auto [ptr, ec]{ std::from_chars(begin, begin + 2, byte, 16) };
                if (ec != std::errc{} || ptr != begin + 2)
                    return false;
                out[i] = static_cast<std::byte>(byte);
            }
            return true;
        }
    } // namespace

    UUID::UUID(std::array<std::byte, 16> bytes) noexcept
        : _bytes{ bytes }
    {
    }

    std::optional<UUID> UUID::fromString(std::string_view str)
    {
        std::array<std::byte, 16> bytes{};
        if (!parseUUID(str, bytes))
            return std::nullopt;
        return UUID{ bytes };
    }

    UUID UUID::fromBytes(std::span<const std::byte, 16> bytes) noexcept
    {
        std::array<std::byte, 16> arr{};
        std::copy(bytes.begin(), bytes.end(), arr.begin());
        return UUID{ arr };
    }

    std::string UUID::toString() const
    {
        static constexpr char hex[]{ "0123456789abcdef" };
        std::string s(36, '-');
        for (std::size_t i{}; i < 16; ++i)
        {
            const auto b{ std::to_integer<unsigned char>(_bytes[i]) };
            s[byteOffsets[i]] = hex[b >> 4];
            s[byteOffsets[i] + 1] = hex[b & 0x0F];
        }
        return s;
    }

    UUID UUID::generate()
    {
        std::uniform_int_distribution<std::uint8_t> dist{ 0, 255 };
        auto& rng{ random::getRandGenerator() };
        std::array<std::byte, binarySize> bytes{};
        for (auto& b : bytes)
            b = static_cast<std::byte>(dist(rng));
        return UUID{ bytes };
    }
} // namespace lms::core
