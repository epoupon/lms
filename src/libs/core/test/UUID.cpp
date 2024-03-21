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

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "core/UUID.hpp"

namespace lms::core
{
    TEST(UUID, caseInsensitive)
    {
        const std::optional<UUID> uuid1{ UUID::fromString("3f51c839-bee2-4e9d-a7b7-0693e45178fc") };
        const std::optional<UUID> uuid2{ UUID::fromString("3f51C839-bEE2-4e9d-a7B7-0693e45178fC") };

        EXPECT_EQ(uuid1, uuid2);
        EXPECT_TRUE(uuid1 >= uuid2);
        EXPECT_TRUE(uuid1 <= uuid2);
    }
}