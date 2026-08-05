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

#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <type_traits>

namespace lms::core
{
    template<typename H>
    concept Hasher = requires(H& hasher, std::span<const std::byte> bytes) {
        { hasher.update(bytes) } -> std::same_as<void>;
        { hasher.digest() } -> std::convertible_to<std::uint64_t>;
    };

    template<Hasher H, typename T>
        requires std::has_unique_object_representations_v<T>
    void hashAppend(H& hasher, const T& value)
    {
        hasher.update(std::as_bytes(std::span{ &value, 1 }));
    }

    template<Hasher H, std::ranges::contiguous_range R>
        requires std::has_unique_object_representations_v<std::ranges::range_value_t<R>>
    void hashAppend(H& hasher, const R& range)
    {
        hashAppend(hasher, std::ranges::size(range));
        hasher.update(std::as_bytes(std::span{ std::ranges::data(range), std::ranges::size(range) }));
    }
} // namespace lms::core
