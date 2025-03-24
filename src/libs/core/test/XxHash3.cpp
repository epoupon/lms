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

#include <gtest/gtest.h>

#include "core/XxHash3.hpp"

namespace lms::core
{
    TEST(Xxhash3_64, basic)
    {
        std::vector<std::byte> buffer;
        buffer.resize(1024);

        for (std::size_t i{}; i < buffer.size(); ++i)
            buffer[i] = static_cast<std::byte>(i);

        const std::uint64_t hash{ xxHash3_64(buffer) };
        EXPECT_EQ(hash, 12137474952470826274ULL);
    }
} // namespace lms::core