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

#include "core/XxHash3.hpp"
#include "core/Exception.hpp"

#define XXH_INLINE_ALL
#include <xxhash.h>

namespace lms::core
{
    std::uint64_t XxHash3_64::hash(std::span<const std::byte> buf)
    {
        return XXH3_64bits(buf.data(), buf.size());
    }

    XxHash3_64::XxHash3_64()
        : _state{ XXH3_createState() }
    {
        if (!_state)
            throw LmsException{ "XXH3_createState failed: out of memory" };
        XXH3_64bits_reset(static_cast<XXH3_state_t*>(_state));
    }

    XxHash3_64::~XxHash3_64()
    {
        XXH3_freeState(static_cast<XXH3_state_t*>(_state));
    }

    void XxHash3_64::update(std::span<const std::byte> buf)
    {
        XXH3_64bits_update(static_cast<XXH3_state_t*>(_state), buf.data(), buf.size());
    }

    std::uint64_t XxHash3_64::digest() const
    {
        return XXH3_64bits_digest(static_cast<const XXH3_state_t*>(_state));
    }
} // namespace lms::core