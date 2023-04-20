/*
 * Copyright (C) 2019 Emeric Poupon
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

#include "utils/EnumSet.hpp"

TEST(EnumSet, ctr)
{
	enum class Foo
	{
		One,
		Two,
	};

	{
		constexpr EnumSet<Foo> test {Foo::One};

		static_assert(!test.empty());
		static_assert(test.contains(Foo::One));
		static_assert(!test.contains(Foo::Two));

		EXPECT_TRUE(!test.empty());
		EXPECT_TRUE(test.contains(Foo::One));
		EXPECT_FALSE(test.contains(Foo::Two));

		static_assert(test.getBitfield() != 0);
	}

	{
		constexpr EnumSet<Foo> test {Foo::One, Foo::Two};
		constexpr auto bitfield {test.getBitfield()};

		EnumSet<Foo> test2;
		EXPECT_FALSE(test2.contains(Foo::One));
		EXPECT_FALSE(test2.contains(Foo::Two));

		test2.setBitfield(bitfield);

		EXPECT_TRUE(test2.contains(Foo::One));
		EXPECT_TRUE(test2.contains(Foo::Two));
		EXPECT_EQ(test, test2);
	}
}
