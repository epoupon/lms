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

#include <iostream>
#include <sstream>

#include <benchmark/benchmark.h>

#include "metadata/Lyrics.hpp"

namespace lms::metadata::benchmarks
{
    static void BM_Lyrics(benchmark::State& state)
    {
        std::istringstream lyricsStream{ R"(
[id: rkrzmqos]
[ar: Billie Eilish]
[al: HIT ME HARD AND SOFT]
[ti: WILDFLOWER]
[length: 04:21]
[00:15.16]Things fall apart and time breaks your heart
[00:21.57]I wasn't there, but I know
[00:28.01]She was your girl, you showed her the world
[00:34.18]You fell out of love and you both let go
[00:40.33]She was cryin' on my shoulder, all I could do was hold her
[00:46.71]Only made us closer until July
[00:53.47]Now I know that you love me, you don't need to remind me
[00:59.86]I should put it all behind me, shouldn't I?
[01:04.72]But I see her in the back of my mind
[01:11.50]All the time
[01:17.75]Like a fever, like I'm burning alive
[01:24.31]Like a sign
[01:32.67]Did I cross the line?
[01:37.72]Mm, hm
[01:48.97]Well, good things don't last (good things don't last)
[01:52.28]And life moves so fast (life moves so fast)
[01:55.50]I'd never ask who was better (I'd never ask who was better)
[02:01.68]'Cause she couldn't be (she couldn't be)
[02:05.20]More different from me (more different)
[02:08.53]Happy and free (happy and free) in leather
[02:14.51]And I know that you love me (you love me)
[02:18.01]You don't need to remind me (remind me)
[02:20.88]Wanna put it all behind me, but baby
[02:26.41]I see her in the back of my mind (back of my mind)
[02:32.66]All the time (all the time)
[02:38.95]Feels like a fever (like a fever)
[02:42.06]Like I'm burning alive (burning alive)
[02:45.66]Like a sign
[02:53.68]Did I cross the line?
[02:58.03]You say no one knows you so well (oh)
[03:02.63]But every time you touch me, I just wonder how she felt
[03:08.54]Valentine's Day, cryin' in the hotel
[03:14.48]I know you didn't mean to hurt me, so I kept it to myself
[03:21.13]And I wonder
[03:24.41]Do you see her in the back of your mind?
[03:31.13]In my eyes?
[03:51.94]You say no one knows you so well
[03:56.88]But every time you touch me, I just wonder how she felt
[04:03.87]Valentine's Day, cryin' in the hotel
[04:09.16]I know you didn't mean to hurt me, so I kept it to myself
[04:15.59]
)" };

        for (auto _ : state)
        {
            lyricsStream.clear();
            lyricsStream.seekg(0, std::ios::beg);

            const Lyrics lyrics{ parseLyrics(lyricsStream) };
            assert(lyrics.synchronizedLines.size() == 41);
        }
    }

    BENCHMARK(BM_Lyrics);

} // namespace lms::metadata::benchmarks

BENCHMARK_MAIN();