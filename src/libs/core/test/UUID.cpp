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

#include <unordered_set>

#include <gtest/gtest.h>

#include "core/UUID.hpp"

namespace lms::core
{
    TEST(UUID, fromString_invalid)
    {
        EXPECT_FALSE(UUID::fromString(""));
        EXPECT_FALSE(UUID::fromString("not-a-uuid"));
        EXPECT_FALSE(UUID::fromString("3f51c839-bee2-4e9d-a7b7-0693e45178f"));   // too short
        EXPECT_FALSE(UUID::fromString("3f51c839-bee2-4e9d-a7b7-0693e45178fcc")); // too long
        EXPECT_FALSE(UUID::fromString("3f51c839-bee2-4e9d-a7b7_0693e45178fc"));  // wrong separator position 23
        EXPECT_FALSE(UUID::fromString("3f51c839-bee2-4e9d-a7b7-0693e45178gz"));  // invalid hex char 'g','z'
        EXPECT_FALSE(UUID::fromString("3f51c839Xbee2-4e9d-a7b7-0693e45178fc"));  // dash replaced at position 8
        EXPECT_FALSE(UUID::fromString("3f51c839-bee2X4e9d-a7b7-0693e45178fc"));  // dash replaced at position 13
        EXPECT_FALSE(UUID::fromString("3f51c839-bee2-4e9dXa7b7-0693e45178fc"));  // dash replaced at position 18
    }

    TEST(UUID, caseInsensitive)
    {
        const std::optional<UUID> uuid1{ UUID::fromString("3f51c839-bee2-4e9d-a7b7-0693e45178fc") };
        const std::optional<UUID> uuid2{ UUID::fromString("3f51C839-bEE2-4e9d-a7B7-0693e45178fC") };

        EXPECT_EQ(uuid1, uuid2);
        EXPECT_TRUE(uuid1 >= uuid2);
        EXPECT_TRUE(uuid1 <= uuid2);
    }

    TEST(UUID, toString_roundTrip)
    {
        const std::string str{ "3f51c839-bee2-4e9d-a7b7-0693e45178fc" };
        const std::optional<UUID> uuid{ UUID::fromString(str) };

        ASSERT_TRUE(uuid);
        EXPECT_EQ(uuid->toString(), str);
    }

    TEST(UUID, toString_lowercase)
    {
        const std::optional<UUID> uuid{ UUID::fromString("3F51C839-BEE2-4E9D-A7B7-0693E45178FC") };

        ASSERT_TRUE(uuid);
        EXPECT_EQ(uuid->toString(), "3f51c839-bee2-4e9d-a7b7-0693e45178fc");
    }

    TEST(UUID, fromBytes_roundTrip)
    {
        const std::optional<UUID> uuid{ UUID::fromString("550e8400-e29b-41d4-a716-446655440000") };
        ASSERT_TRUE(uuid);

        const UUID fromB{ UUID::fromBytes(uuid->bytes()) };
        EXPECT_EQ(fromB, *uuid);
        EXPECT_EQ(fromB.toString(), "550e8400-e29b-41d4-a716-446655440000");
    }

    TEST(UUID, bytes_size)
    {
        const std::optional<UUID> uuid{ UUID::fromString("3f51c839-bee2-4e9d-a7b7-0693e45178fc") };
        ASSERT_TRUE(uuid);
        EXPECT_EQ(uuid->bytes().size(), 16U);
    }

    TEST(UUID, generate_validString)
    {
        const UUID uuid{ UUID::generate() };
        const std::string s{ uuid.toString() };

        ASSERT_EQ(s.size(), 36U);
        EXPECT_EQ(s[8], '-');
        EXPECT_EQ(s[13], '-');
        EXPECT_EQ(s[18], '-');
        EXPECT_EQ(s[23], '-');
        EXPECT_TRUE(UUID::fromString(s).has_value());
    }
} // namespace lms::core