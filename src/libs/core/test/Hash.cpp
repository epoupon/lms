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

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "core/DefaultHasher.hpp"
#include "core/Hash.hpp"

namespace lms::core::tests
{
    static_assert(Hasher<DefaultHasher>);

    enum class TestEnum
    {
        A,
        B,
    };

    TEST(Hash, sameValueProducesSameDigest)
    {
        DefaultHasher hasher1;
        DefaultHasher hasher2;

        hashAppend(hasher1, 42);
        hashAppend(hasher2, 42);

        EXPECT_EQ(hasher1.digest(), hasher2.digest());
    }

    TEST(Hash, differentValueProducesDifferentDigest)
    {
        DefaultHasher hasher1;
        DefaultHasher hasher2;

        hashAppend(hasher1, 42);
        hashAppend(hasher2, 43);

        EXPECT_NE(hasher1.digest(), hasher2.digest());
    }

    TEST(Hash, orderMatters)
    {
        DefaultHasher hasher1;
        DefaultHasher hasher2;

        hashAppend(hasher1, 1);
        hashAppend(hasher1, 2);

        hashAppend(hasher2, 2);
        hashAppend(hasher2, 1);

        EXPECT_NE(hasher1.digest(), hasher2.digest());
    }

    TEST(Hash, worksOnEnums)
    {
        DefaultHasher hasher1;
        DefaultHasher hasher2;

        hashAppend(hasher1, TestEnum::A);
        hashAppend(hasher2, TestEnum::B);

        EXPECT_NE(hasher1.digest(), hasher2.digest());
    }

    TEST(Hash, matchesDirectSpanUpdate)
    {
        const int value{ 1234 };

        DefaultHasher hasher1;
        hashAppend(hasher1, value);

        DefaultHasher hasher2;
        hasher2.update(std::as_bytes(std::span{ &value, 1 }));

        EXPECT_EQ(hasher1.digest(), hasher2.digest());
    }

    TEST(Hash, sameStringProducesSameDigest)
    {
        DefaultHasher hasher1;
        DefaultHasher hasher2;

        hashAppend(hasher1, std::string{ "hello" });
        hashAppend(hasher2, std::string{ "hello" });

        EXPECT_EQ(hasher1.digest(), hasher2.digest());
    }

    TEST(Hash, differentStringProducesDifferentDigest)
    {
        DefaultHasher hasher1;
        DefaultHasher hasher2;

        hashAppend(hasher1, std::string{ "hello" });
        hashAppend(hasher2, std::string{ "world" });

        EXPECT_NE(hasher1.digest(), hasher2.digest());
    }

    TEST(Hash, stringLengthIsDisambiguated)
    {
        // without a size prefix, "ab"+"c" and "a"+"bc" would hash identically
        DefaultHasher hasher1;
        hashAppend(hasher1, std::string{ "ab" });
        hashAppend(hasher1, std::string{ "c" });

        DefaultHasher hasher2;
        hashAppend(hasher2, std::string{ "a" });
        hashAppend(hasher2, std::string{ "bc" });

        EXPECT_NE(hasher1.digest(), hasher2.digest());
    }

    TEST(Hash, vectorOfIntHashing)
    {
        DefaultHasher hasher1;
        DefaultHasher hasher2;
        DefaultHasher hasher3;

        hashAppend(hasher1, std::vector<int>{ 1, 2, 3 });
        hashAppend(hasher2, std::vector<int>{ 1, 2, 3 });
        hashAppend(hasher3, std::vector<int>{ 1, 2, 4 });

        EXPECT_EQ(hasher1.digest(), hasher2.digest());
        EXPECT_NE(hasher1.digest(), hasher3.digest());
    }
} // namespace lms::core::tests
