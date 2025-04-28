/*
 * Copyright (C) 2025 Emeric Poupon
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

namespace lms::core
{
    template<typename Tag, typename T>
    class TaggedType
    {
    public:
        using underlying_type = T;

        explicit constexpr TaggedType() = default;
        explicit constexpr TaggedType(T value)
            : _value{ value } {}

        constexpr T value() const { return _value; }

        auto operator<=>(const TaggedType&) const = default;

    private:
        T _value{};
    };

    template<typename Tag>
    using TaggedBool = TaggedType<Tag, bool>;
} // namespace lms::core