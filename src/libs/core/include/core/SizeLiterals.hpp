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

#include <cstddef>

namespace lms::core::literals
{
    constexpr std::size_t operator""_KiB(unsigned long long int x)
    {
        return 1024ULL * x;
    }

    constexpr std::size_t operator""_MiB(unsigned long long int x)
    {
        return 1024_KiB * x;
    }

    constexpr std::size_t operator""_GiB(unsigned long long int x)
    {
        return 1024_MiB * x;
    }

    constexpr std::size_t operator""_TiB(unsigned long long int x)
    {
        return 1024_GiB * x;
    }

    constexpr std::size_t operator""_PiB(unsigned long long int x)
    {
        return 1024_TiB * x;
    }
} // namespace lms::core::literals