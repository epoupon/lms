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

#include "core/Md5.hpp"

#include <Wt/Utils.h>

namespace lms::core
{
    std::array<std::byte, 16> md5(std::string_view data)
    {
        const std::string raw{ Wt::Utils::md5(std::string{ data }) };

        std::array<std::byte, 16> result;
        for (std::size_t i{}; i < 16; ++i)
            result[i] = static_cast<std::byte>(static_cast<unsigned char>(raw[i]));
        return result;
    }
} // namespace lms::core
