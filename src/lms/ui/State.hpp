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

#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "core/String.hpp"

namespace lms::ui::state
{
    namespace details
    {
        std::optional<std::string> readValue(std::string_view item);
        void writeValue(std::string_view item, std::string_view value);
        void eraseValue(std::string_view item);
    } // namespace details

    template<typename T>
    void writeValue(std::string_view item, std::optional<T> value)
    {
        if (value.has_value())
        {
            if constexpr (std::is_enum_v<T>)
                details::writeValue(item, std::to_string(static_cast<std::underlying_type_t<T>>(*value)));
            else
                details::writeValue(item, std::to_string(*value));
        }
        else
            details::eraseValue(item);
    }

    template<typename T>
    std::optional<T> readValue(std::string_view item)
    {
        if (std::optional<std::string> res{ details::readValue(item) })
            return core::stringUtils::readAs<T>(*res);

        return std::nullopt;
    }

} // namespace lms::ui::state
