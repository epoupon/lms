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

#include "core/http/UrlValidation.hpp"

namespace lms::core::http::tests
{
    TEST(HttpUrlValidation, AllowedUrls)
    {
        EXPECT_TRUE(isValidUrl("http://feeds.example.com/podcast.rss"));
        EXPECT_TRUE(isValidUrl("https://feeds.example.com/podcast.rss"));
        EXPECT_TRUE(isValidUrl("http://192.168.1.100/feed.rss"));
        EXPECT_TRUE(isValidUrl("https://example.com/episode.mp3"));
    }

    TEST(HttpUrlValidation, DisallowedUrls)
    {
        EXPECT_FALSE(isValidUrl(""));
        EXPECT_FALSE(isValidUrl("ftp://attacker.com/feed"));
        EXPECT_FALSE(isValidUrl("file:///etc/passwd"));
        EXPECT_FALSE(isValidUrl("javascript:alert(1)"));
        EXPECT_FALSE(isValidUrl("//example.com/feed"));
        EXPECT_FALSE(isValidUrl("HTTP://example.com/feed")); // scheme check is case-sensitive
    }
} // namespace lms::core::http::tests
