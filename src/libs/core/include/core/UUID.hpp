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

#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "core/String.hpp"

namespace lms::core
{
    class UUID
    {
    public:
        static constexpr std::size_t binarySize{ 16 };

        UUID() noexcept = default;
        static std::optional<UUID> fromString(std::string_view str);
        static UUID fromBytes(std::span<const std::byte, binarySize> bytes) noexcept;
        static UUID generate();

        std::string toString() const;
        std::span<const std::byte, binarySize> bytes() const noexcept { return _bytes; }

        auto operator<=>(const UUID&) const = default;

    private:
        explicit UUID(std::array<std::byte, binarySize> bytes) noexcept;
        std::array<std::byte, binarySize> _bytes{};
    };
} // namespace lms::core

namespace lms::core::stringUtils
{
    template<>
    std::optional<UUID> readAs(std::string_view str);
}

namespace std
{
    template<>
    struct hash<lms::core::UUID>
    {
        size_t operator()(const lms::core::UUID& uuid) const noexcept
        {
            const auto& b{ uuid.bytes() };
            return hash<string_view>{}({ static_cast<const char*>(static_cast<const void*>(b.data())), b.size() });
        }
    };
} // namespace std
