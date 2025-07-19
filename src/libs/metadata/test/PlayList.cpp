
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

#include <gtest/gtest.h>
#include <sstream>

#include "metadata/PlayList.hpp"

namespace lms::metadata::tests
{
    TEST(PlayList, basic)
    {
        std::istringstream is{ R"(#EXTM3U
#PLAYLIST:My super playlist  
 01-Foo.mp3

    

#EXTINF:263,Alice in Chains - Don't Follow
02-FooBar.mp3
#EXTALB:Album Title (2009)
03-Bar.mp3
/this is/a test with a long/path and some spaces/foo.mp3
and another one/with relative path/foo.mp3
one to be/../one to be/normalized/foo.mp3)" };

        const PlayList playlist{ parsePlayList(is) };

        EXPECT_EQ(playlist.name, "My super playlist");
        ASSERT_EQ(playlist.files.size(), 6);
        EXPECT_EQ(playlist.files[0], "01-Foo.mp3");
        EXPECT_EQ(playlist.files[1], "02-FooBar.mp3");
        EXPECT_EQ(playlist.files[2], "03-Bar.mp3");
        EXPECT_EQ(playlist.files[3], "/this is/a test with a long/path and some spaces/foo.mp3");
        EXPECT_EQ(playlist.files[4], "and another one/with relative path/foo.mp3");
        EXPECT_EQ(playlist.files[5], "one to be/normalized/foo.mp3");
    }

    TEST(PlayList, UTF8_bom)
    {
        const unsigned char content[] = { 0xEF, 0xBB, 0xBF, '#', 'E', 'X', 'T', 'M', '3', 'U', '\r', '\n', '\r', '\n', '.', '.', '/', 't', 'e', 's', 't', '.', 'm', 'p', '3', '\r', '\n' };
        std::istringstream is{ std::string(reinterpret_cast<const char*>(content), sizeof(content)) };

        const PlayList playlist{ parsePlayList(is) };
        EXPECT_EQ(playlist.name, "");
        ASSERT_EQ(playlist.files.size(), 1);
        EXPECT_EQ(playlist.files[0], "../test.mp3");
    }

} // namespace lms::metadata::tests