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
#include <cstdint>
#include <span>

namespace lms::core
{
    class XxHash3_64
    {
    public:
        static std::uint64_t hash(std::span<const std::byte> buf);

        XxHash3_64();
        ~XxHash3_64();
        XxHash3_64(const XxHash3_64&) = delete;
        XxHash3_64& operator=(const XxHash3_64&) = delete;

        void update(std::span<const std::byte> buf);
        std::uint64_t digest() const;

    private:
        void* _state{};
    };
} // namespace lms::core