/*
 * copyright (c) 2021 emeric poupon
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

#include <optional>
#include <string_view>

#include "core/String.hpp"

namespace lms::api::subsonic
{
    struct ProtocolVersion
    {
        unsigned major{};
        unsigned minor{};
        unsigned patch{};
    };

    static inline constexpr ProtocolVersion defaultServerProtocolVersion{ 1, 16, 0 };
    static inline constexpr std::string_view serverVersion{ "8" };
} // namespace lms::api::subsonic

namespace lms::core::stringUtils
{
    template<>
    std::optional<api::subsonic::ProtocolVersion> readAs(std::string_view str);
}
