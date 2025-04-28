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

#include "core/PartialDateTime.hpp"

namespace lms::core::stringUtils::tests
{
    TEST(PartialDateTime, year)
    {
        EXPECT_EQ(PartialDateTime{}.getYear(), std::nullopt);
        EXPECT_EQ(PartialDateTime{ 1992 }.getYear(), 1992);
    }

    TEST(PartialDateTime, month)
    {
        EXPECT_EQ(PartialDateTime{}.getMonth(), std::nullopt);
        EXPECT_EQ((PartialDateTime{ 1992, 3 }.getMonth()), std::optional<int>{ 3 });
    }
    TEST(PartialDateTime, day)
    {
        EXPECT_EQ(PartialDateTime{}.getDay(), std::nullopt);
        EXPECT_EQ((PartialDateTime{ 1992, 3, 27 }.getDay()), std::optional<int>{ 27 });
    }

    TEST(PartialDateTime, comparison)
    {
        EXPECT_EQ((PartialDateTime{ 1992, 3, 27 }), (PartialDateTime{ 1992, 3, 27 }));
        EXPECT_EQ((PartialDateTime{ 1992, 3 }), (PartialDateTime{ 1992, 3 }));
        EXPECT_EQ(PartialDateTime{ 1992 }, PartialDateTime{ 1992 });
        EXPECT_NE((PartialDateTime{ 1992, 3, 27 }), (PartialDateTime{ 1992, 3 }));
        EXPECT_NE((PartialDateTime{ 1992, 3, 27 }), (PartialDateTime{ 1992 }));
        EXPECT_NE((PartialDateTime{ 1992, 3 }), (PartialDateTime{ 1992 }));
        EXPECT_NE((PartialDateTime{ 1992, 3, 27 }), (PartialDateTime{ 1992, 3, 28 }));
        EXPECT_NE((PartialDateTime{ 1992, 3, 27 }), (PartialDateTime{ 1992, 4, 27 }));
        EXPECT_NE((PartialDateTime{ 1992, 3, 27 }), (PartialDateTime{ 1993, 3, 27 }));
        EXPECT_GT((PartialDateTime{ 1993, 3, 28 }), (PartialDateTime{ 1993, 3, 27 }));
        EXPECT_GT((PartialDateTime{ 1993, 4 }), (PartialDateTime{ 1993, 3, 27 }));
        EXPECT_GT((PartialDateTime{ 1994 }), (PartialDateTime{ 1993, 3, 27 }));
        EXPECT_LT((PartialDateTime{ 1993, 3, 27 }), (PartialDateTime{ 1993, 3, 28 }));
        EXPECT_LT((PartialDateTime{ 1993, 3, 27 }), (PartialDateTime{ 1993, 4 }));
        EXPECT_LT((PartialDateTime{ 1993, 3, 27 }), (PartialDateTime{ 1994 }));
    }

    TEST(PartialDateTime, stringComparison)
    {
        EXPECT_EQ((PartialDateTime{ 1992, 3, 27 }.toString()), (PartialDateTime{ 1992, 3, 27 }.toString()));
        EXPECT_EQ((PartialDateTime{ 1992, 3 }.toString()), (PartialDateTime{ 1992, 3 }.toString()));
        EXPECT_EQ(PartialDateTime{ 1992 }.toString(), PartialDateTime{ 1992 }.toString());
        EXPECT_NE((PartialDateTime{ 1992, 3, 27 }.toString()), (PartialDateTime{ 1992, 3 }.toString()));
        EXPECT_NE((PartialDateTime{ 1992, 3, 27 }.toString()), (PartialDateTime{ 1992 }.toString()));
        EXPECT_NE((PartialDateTime{ 1992, 3 }.toString()), (PartialDateTime{ 1992 }.toString()));
        EXPECT_NE((PartialDateTime{ 1992, 3, 27 }.toString()), (PartialDateTime{ 1992, 3, 28 }.toString()));
        EXPECT_NE((PartialDateTime{ 1992, 3, 27 }.toString()), (PartialDateTime{ 1992, 4, 27 }.toString()));
        EXPECT_NE((PartialDateTime{ 1992, 3, 27 }.toString()), (PartialDateTime{ 1993, 3, 27 }.toString()));
        EXPECT_GT((PartialDateTime{ 1993, 3, 28 }.toString()), (PartialDateTime{ 1993, 3, 27 }.toString()));
        EXPECT_GT((PartialDateTime{ 1993, 4 }.toString()), (PartialDateTime{ 1993, 3, 27 }.toString()));
        EXPECT_GT((PartialDateTime{ 1994 }.toString()), (PartialDateTime{ 1993, 3, 27 }.toString()));
        EXPECT_LT((PartialDateTime{ 1993, 3, 27 }.toString()), (PartialDateTime{ 1993, 3, 28 }.toString()));
        EXPECT_LT((PartialDateTime{ 1993, 3, 27 }.toString()), (PartialDateTime{ 1993, 4 }.toString()));
        EXPECT_LT((PartialDateTime{ 1993, 3, 27 }.toString()), (PartialDateTime{ 1994 }.toString()));
    }

    TEST(PartialDateTime, stringConversions)
    {
        struct TestCase
        {
            std::string_view input;
            std::string_view expectedOutput;
        };

        constexpr TestCase tests[]{
            { "", "" },
            { "aaaa", "" },
            { "aaaa-bb", "" },
            { "1992", "1992" },
            { "1992-03", "1992-03" },
            { "1992-00", "1992" },
            { "1992-03-00", "1992-03" },
            { "1992-03-27", "1992-03-27" },
            { "1992-00-00", "1992" },
            { "1992-03-27T15", "1992-03-27T15" },
            { "1992-03-27T15:08", "1992-03-27T15:08" },
            { "1992-03-27T15:08:57", "1992-03-27T15:08:57" },
            { "1992-03-00T15:08:57", "1992-03" },

            { "1992-03-27 15", "1992-03-27T15" },
            { "1992-03-27 15:08", "1992-03-27T15:08" },
            { "1992-03-27 15:08:57", "1992-03-27T15:08:57" },

            { "aaaa/bb", "" },
            { "1992", "1992" },
            { "1992/00", "1992" },
            { "1992/03", "1992-03" },
            { "1992/03/00", "1992-03" },
            { "1992/03/27", "1992-03-27" },
            { "1992/03/27 15", "1992-03-27T15" },
            { "1992/03/27 15:08", "1992-03-27T15:08" },
            { "1992/03/27 15:08:57", "1992-03-27T15:08:57" },
            { "1992/03/00 15:08:57", "1992-03" },
        };

        for (const TestCase& test : tests)
        {
            const PartialDateTime dateTime{ PartialDateTime::fromString(test.input) };
            EXPECT_EQ(dateTime.toString(), test.expectedOutput) << "Input = '" << test.input;
        }
    }
} // namespace lms::core::stringUtils::tests
