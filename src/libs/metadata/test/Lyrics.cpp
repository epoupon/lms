
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

#include "metadata/Lyrics.hpp"

namespace lms::metadata::tests
{
    using namespace std::chrono_literals;

    TEST(Lyrics, synchronized)
    {
        std::istringstream is{ R"([id: dqsxdkbu]
[ar: Lady Gaga]
[al: Lady Gaga]
[ti: Die With A Smile]
[la: eng]
[length: 04:12]
[offset: -34]
[00:03.30]Ooh, ooh
[00:06.75]
[00:09.16]I, I just woke up from a dream)" };

        const Lyrics lyrics{ parseLyrics(is) };

        EXPECT_EQ(lyrics.displayArtist, "Lady Gaga");
        EXPECT_EQ(lyrics.displayAlbum, "Lady Gaga");
        EXPECT_EQ(lyrics.displayTitle, "Die With A Smile");
        EXPECT_EQ(lyrics.language, "eng");
        EXPECT_EQ(lyrics.offset, -34ms);
        EXPECT_EQ(lyrics.unsynchronizedLines.size(), 0);
        ASSERT_EQ(lyrics.synchronizedLines.size(), 3);
        ASSERT_TRUE(lyrics.synchronizedLines.contains(3s + 300ms));
        EXPECT_EQ(lyrics.synchronizedLines.find(3s + 300ms)->second, "Ooh, ooh");
        ASSERT_TRUE(lyrics.synchronizedLines.contains(6s + 750ms));
        EXPECT_EQ(lyrics.synchronizedLines.find(6s + 750ms)->second, "");
        ASSERT_TRUE(lyrics.synchronizedLines.contains(9s + 160ms));
        EXPECT_EQ(lyrics.synchronizedLines.find(9s + 160ms)->second, "I, I just woke up from a dream");
    }

    TEST(Lyrics, tagInMidleOfLyrics)
    {
        std::istringstream is{ R"([00:03.30]Ooh, ooh
[id: dqsxdkbu]
[00:09.16]I, I just woke up from a dream)" };

        const Lyrics lyrics{ parseLyrics(is) };

        EXPECT_EQ(lyrics.unsynchronizedLines.size(), 0);
        ASSERT_EQ(lyrics.synchronizedLines.size(), 2);
        ASSERT_TRUE(lyrics.synchronizedLines.contains(3s + 300ms));
        EXPECT_EQ(lyrics.synchronizedLines.find(3s + 300ms)->second, "Ooh, ooh");
        ASSERT_TRUE(lyrics.synchronizedLines.contains(9s + 160ms));
        EXPECT_EQ(lyrics.synchronizedLines.find(9s + 160ms)->second, "I, I just woke up from a dream");
    }

    TEST(Lyrics, tagAtTheEndOfLyrics)
    {
        std::istringstream is{ R"([00:03.30]Ooh, ooh
[00:09.16]I, I just woke up from a dream
[id: dqsxdkbu])" };

        const Lyrics lyrics{ parseLyrics(is) };

        EXPECT_EQ(lyrics.unsynchronizedLines.size(), 0);
        ASSERT_EQ(lyrics.synchronizedLines.size(), 2);
        ASSERT_TRUE(lyrics.synchronizedLines.contains(3s + 300ms));
        EXPECT_EQ(lyrics.synchronizedLines.find(3s + 300ms)->second, "Ooh, ooh");
        ASSERT_TRUE(lyrics.synchronizedLines.contains(9s + 160ms));
        EXPECT_EQ(lyrics.synchronizedLines.find(9s + 160ms)->second, "I, I just woke up from a dream");
    }

    TEST(Lyrics, skipEmptyBeginLines)
    {
        std::istringstream is{ R"(

  
[00:03.30]Ooh, ooh)" };
        const Lyrics lyrics{ parseLyrics(is) };

        EXPECT_EQ(lyrics.unsynchronizedLines.size(), 0);
        ASSERT_EQ(lyrics.synchronizedLines.size(), 1);
        ASSERT_TRUE(lyrics.synchronizedLines.contains(3s + 300ms));
        EXPECT_EQ(lyrics.synchronizedLines.find(3s + 300ms)->second, "Ooh, ooh");
    }

    TEST(Lyrics, skipEmptyEndLines)
    {
        std::istringstream is{ R"([00:03.30]Ooh, ooh

)" };
        const Lyrics lyrics{ parseLyrics(is) };

        EXPECT_EQ(lyrics.unsynchronizedLines.size(), 0);
        ASSERT_EQ(lyrics.synchronizedLines.size(), 1);
        ASSERT_TRUE(lyrics.synchronizedLines.contains(3s + 300ms));
        EXPECT_EQ(lyrics.synchronizedLines.find(3s + 300ms)->second, "Ooh, ooh");
    }

    TEST(Lyrics, skipLeadingUnsynchronizedLyrics)
    {
        std::istringstream is{ R"(
Some unsynchronized lyrics
[00:03.30]Ooh, ooh)" };

        const Lyrics lyrics{ parseLyrics(is) };

        EXPECT_EQ(lyrics.unsynchronizedLines.size(), 0);
        ASSERT_EQ(lyrics.synchronizedLines.size(), 1);
        ASSERT_TRUE(lyrics.synchronizedLines.contains(3s + 300ms));
        EXPECT_EQ(lyrics.synchronizedLines.find(3s + 300ms)->second, "Ooh, ooh");
    }

    TEST(Lyrics, skipComments)
    {
        std::istringstream is{ R"(###
[00:03.30]Ooh, ooh
## just dance
[00:09.16]I, I just woke up from a dream
##end)" };

        const Lyrics lyrics{ parseLyrics(is) };

        EXPECT_EQ(lyrics.unsynchronizedLines.size(), 0);
        ASSERT_EQ(lyrics.synchronizedLines.size(), 2);
        ASSERT_TRUE(lyrics.synchronizedLines.contains(3s + 300ms));
        EXPECT_EQ(lyrics.synchronizedLines.find(3s + 300ms)->second, "Ooh, ooh");
        ASSERT_TRUE(lyrics.synchronizedLines.contains(9s + 160ms));
        EXPECT_EQ(lyrics.synchronizedLines.find(9s + 160ms)->second, "I, I just woke up from a dream");
    }

    TEST(Lyrics, synchronized_notags)
    {
        std::istringstream is{ R"([00:03.30]Ooh, ooh)" };

        const Lyrics lyrics{ parseLyrics(is) };

        EXPECT_TRUE(lyrics.displayArtist.empty());
        EXPECT_TRUE(lyrics.displayAlbum.empty());
        EXPECT_TRUE(lyrics.displayTitle.empty());
        EXPECT_EQ(lyrics.offset, std::chrono::milliseconds{ 0 });
        EXPECT_EQ(lyrics.unsynchronizedLines.size(), 0);
        ASSERT_EQ(lyrics.synchronizedLines.size(), 1);
        ASSERT_TRUE(lyrics.synchronizedLines.contains(3s + 300ms));
        EXPECT_EQ(lyrics.synchronizedLines.find(3s + 300ms)->second, "Ooh, ooh");
    }

    TEST(Lyrics, synchronized_timestampFormats)
    {
        std::istringstream is{ R"([00:03.30]First line
[00:01.301]in milliseconds
[0:02.301]leading with only one digit
[61:01.30]more than 60 minutes
[02:01:01.30]With hours
[3:01:01.30]With hours with only one digit)" };

        const Lyrics lyrics{ parseLyrics(is) };

        ASSERT_EQ(lyrics.synchronizedLines.size(), 6);
        ASSERT_TRUE(lyrics.synchronizedLines.contains(3s + 300ms));
        ASSERT_TRUE(lyrics.synchronizedLines.contains(1s + 301ms));
        ASSERT_TRUE(lyrics.synchronizedLines.contains(2s + 301ms));
        ASSERT_TRUE(lyrics.synchronizedLines.contains(61min + 1s + 300ms));
        ASSERT_TRUE(lyrics.synchronizedLines.contains(2h + 1min + 1s + 300ms));
        ASSERT_TRUE(lyrics.synchronizedLines.contains(3h + 1min + 1s + 300ms));
    }

    TEST(Lyrics, synchronized_keepBlankLinesExceptEOF)
    {
        std::istringstream is{ R"([00:03.30]Ooh, ooh


[00:06.75]Foo
 )" };

        const Lyrics lyrics{ parseLyrics(is) };

        EXPECT_TRUE(lyrics.displayArtist.empty());
        EXPECT_TRUE(lyrics.displayAlbum.empty());
        EXPECT_TRUE(lyrics.displayTitle.empty());
        EXPECT_EQ(lyrics.offset, std::chrono::milliseconds{ 0 });
        EXPECT_EQ(lyrics.unsynchronizedLines.size(), 0);
        ASSERT_EQ(lyrics.synchronizedLines.size(), 2);
        ASSERT_TRUE(lyrics.synchronizedLines.contains(3s + 300ms));
        EXPECT_EQ(lyrics.synchronizedLines.find(3s + 300ms)->second, "Ooh, ooh\n\n");
        ASSERT_TRUE(lyrics.synchronizedLines.contains(6s + 750ms));
        EXPECT_EQ(lyrics.synchronizedLines.find(6s + 750ms)->second, "Foo");
    }

    TEST(Lyrics, synchronized_blankLinesEnd)
    {
        std::istringstream is{ R"([00:03.30]Ooh, ooh
SecondLine
 Even a third line!!
[00:06.75]Foo
 )" };

        const Lyrics lyrics{ parseLyrics(is) };

        EXPECT_TRUE(lyrics.displayArtist.empty());
        EXPECT_TRUE(lyrics.displayAlbum.empty());
        EXPECT_TRUE(lyrics.displayTitle.empty());
        EXPECT_EQ(lyrics.offset, std::chrono::milliseconds{ 0 });
        EXPECT_EQ(lyrics.unsynchronizedLines.size(), 0);
        ASSERT_EQ(lyrics.synchronizedLines.size(), 2);
        ASSERT_TRUE(lyrics.synchronizedLines.contains(3s + 300ms));
        EXPECT_EQ(lyrics.synchronizedLines.find(3s + 300ms)->second, "Ooh, ooh\nSecondLine\n Even a third line!!");
        ASSERT_TRUE(lyrics.synchronizedLines.contains(6s + 750ms));
        EXPECT_EQ(lyrics.synchronizedLines.find(6s + 750ms)->second, "Foo");
    }

    TEST(Lyrics, synchronized_multitimestamps)
    {
        std::istringstream is{ R"([00:03.30][00:09.16] [00:15.16]Ooh, ooh
[00:06.75]I, I just woke up from a dream)" };

        const Lyrics lyrics{ parseLyrics(is) };

        EXPECT_TRUE(lyrics.displayArtist.empty());
        EXPECT_TRUE(lyrics.displayAlbum.empty());
        EXPECT_TRUE(lyrics.displayTitle.empty());
        EXPECT_EQ(lyrics.offset, std::chrono::milliseconds{ 0 });
        EXPECT_EQ(lyrics.unsynchronizedLines.size(), 0);
        ASSERT_EQ(lyrics.synchronizedLines.size(), 4);
        ASSERT_TRUE(lyrics.synchronizedLines.contains(3s + 300ms));
        EXPECT_EQ(lyrics.synchronizedLines.find(3s + 300ms)->second, "Ooh, ooh");
        ASSERT_TRUE(lyrics.synchronizedLines.contains(6s + 750ms));
        EXPECT_EQ(lyrics.synchronizedLines.find(6s + 750ms)->second, "I, I just woke up from a dream");
        ASSERT_TRUE(lyrics.synchronizedLines.contains(9s + 160ms));
        EXPECT_EQ(lyrics.synchronizedLines.find(9s + 160ms)->second, "Ooh, ooh");
        ASSERT_TRUE(lyrics.synchronizedLines.contains(15s + 160ms));
        EXPECT_EQ(lyrics.synchronizedLines.find(15s + 160ms)->second, "Ooh, ooh");
    }

    TEST(Lyrics, synchronized_multitimestamps_blank)
    {
        std::istringstream is{ R"([00:03.30]Ooh, ooh
[00:06.75]
[00:09.16]I, I just woke up from a dream
[00:10.16]

)" };

        const Lyrics lyrics{ parseLyrics(is) };

        EXPECT_TRUE(lyrics.displayArtist.empty());
        EXPECT_TRUE(lyrics.displayAlbum.empty());
        EXPECT_TRUE(lyrics.displayTitle.empty());
        EXPECT_EQ(lyrics.offset, std::chrono::milliseconds{ 0 });
        EXPECT_EQ(lyrics.unsynchronizedLines.size(), 0);
        ASSERT_EQ(lyrics.synchronizedLines.size(), 4);
        ASSERT_TRUE(lyrics.synchronizedLines.contains(3s + 300ms));
        EXPECT_EQ(lyrics.synchronizedLines.find(3s + 300ms)->second, "Ooh, ooh");
        ASSERT_TRUE(lyrics.synchronizedLines.contains(6s + 750ms));
        EXPECT_EQ(lyrics.synchronizedLines.find(6s + 750ms)->second, "");
        ASSERT_TRUE(lyrics.synchronizedLines.contains(9s + 160ms));
        EXPECT_EQ(lyrics.synchronizedLines.find(9s + 160ms)->second, "I, I just woke up from a dream");
        ASSERT_TRUE(lyrics.synchronizedLines.contains(10s + 160ms));
        EXPECT_EQ(lyrics.synchronizedLines.find(10s + 160ms)->second, "");
    }

    TEST(Lyrics, synchronized_multitimestamps_multilines)
    {
        std::istringstream is{ R"([00:03.30][00:09.16]Ooh, ooh
Second line
 Third line

Fifth line after an empty one...
[00:06.75]I, I just woke up from a dream
Cool)" };

        const Lyrics lyrics{ parseLyrics(is) };

        EXPECT_TRUE(lyrics.displayArtist.empty());
        EXPECT_TRUE(lyrics.displayAlbum.empty());
        EXPECT_TRUE(lyrics.displayTitle.empty());
        EXPECT_EQ(lyrics.offset, std::chrono::milliseconds{ 0 });
        EXPECT_EQ(lyrics.unsynchronizedLines.size(), 0);
        ASSERT_EQ(lyrics.synchronizedLines.size(), 3);
        ASSERT_TRUE(lyrics.synchronizedLines.contains(3s + 300ms));
        EXPECT_EQ(lyrics.synchronizedLines.find(3s + 300ms)->second, "Ooh, ooh\nSecond line\n Third line\n\nFifth line after an empty one...");
        ASSERT_TRUE(lyrics.synchronizedLines.contains(6s + 750ms));
        EXPECT_EQ(lyrics.synchronizedLines.find(6s + 750ms)->second, "I, I just woke up from a dream\nCool");
        ASSERT_TRUE(lyrics.synchronizedLines.contains(9s + 160ms));
        EXPECT_EQ(lyrics.synchronizedLines.find(9s + 160ms)->second, "Ooh, ooh\nSecond line\n Third line\n\nFifth line after an empty one...");
    }

    TEST(Lyrics, unsynchronized)
    {
        std::istringstream is{ R"([id: dqsxdkbu]
[ar: Lady Gaga]
[al: Lady Gaga]
[ti: Die With A Smile]
[length: 04:12]
[offset: -34]
Ooh, ooh


I, I just woke up from a dream

)" };

        Lyrics lyrics{ parseLyrics(is) };

        EXPECT_EQ(lyrics.displayArtist, "Lady Gaga");
        EXPECT_EQ(lyrics.displayAlbum, "Lady Gaga");
        EXPECT_EQ(lyrics.displayTitle, "Die With A Smile");
        EXPECT_EQ(lyrics.offset, -34ms);
        ASSERT_EQ(lyrics.unsynchronizedLines.size(), 5);
        EXPECT_EQ(lyrics.unsynchronizedLines[0], "Ooh, ooh");
        EXPECT_EQ(lyrics.unsynchronizedLines[1], "");
        EXPECT_EQ(lyrics.unsynchronizedLines[2], "");
        EXPECT_EQ(lyrics.unsynchronizedLines[3], "I, I just woke up from a dream");
        EXPECT_EQ(lyrics.unsynchronizedLines[4], "");
    }
} // namespace lms::metadata::tests