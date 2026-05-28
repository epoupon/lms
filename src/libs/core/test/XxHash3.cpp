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

        const std::uint64_t hash{ XxHash3_64::hash(buffer) };
        EXPECT_EQ(hash, 12137474952470826274ULL);
    }

    TEST(Xxhash3_64, streamingMatchesOneShot)
    {
        std::vector<std::byte> buffer;
        buffer.resize(1024);

        for (std::size_t i{}; i < buffer.size(); ++i)
            buffer[i] = static_cast<std::byte>(i);

        const std::uint64_t expected{ XxHash3_64::hash(buffer) };

        // Feed the same data in three unequal chunks to exercise the streaming path
        constexpr std::size_t chunk1{ 100 };
        constexpr std::size_t chunk2{ 400 };
        XxHash3_64 hasher;
        hasher.update(std::span{ buffer }.subspan(0, chunk1));
        hasher.update(std::span{ buffer }.subspan(chunk1, chunk2));
        hasher.update(std::span{ buffer }.subspan(chunk1 + chunk2));
        EXPECT_EQ(hasher.digest(), expected);
    }
} // namespace lms::core