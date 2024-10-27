/*
 * Copyright (C) 2021 Emeric Poupon
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

#include "Common.hpp"

#include "database/Track.hpp"
#include "database/TrackLyrics.hpp"

namespace lms::db::tests
{
    using ScopedTrackLyrics = ScopedEntity<db::TrackLyrics>;

    TEST_F(DatabaseFixture, TrackLyrics_synchronized)
    {
        using namespace std::chrono_literals;

        ScopedTrack track{ session };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(TrackLyrics::getCount(session), 0);
        }

        ScopedTrackLyrics lyrics{ session };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(TrackLyrics::getCount(session), 1);
            EXPECT_EQ(TrackLyrics::getExternalLyricsCount(session), 0);

            const TrackLyrics::pointer dbLyrics{ lyrics.get() };
            EXPECT_FALSE(dbLyrics->isSynchronized());
            EXPECT_FALSE(dbLyrics->getLastWriteTime().isValid());
            EXPECT_TRUE(dbLyrics->getLastWriteTime().isNull());
        }

        {
            auto transaction{ session.createWriteTransaction() };

            TrackLyrics::pointer dbLyrics{ lyrics.get() };

            dbLyrics.modify()->setSynchronizedLines({ { 1s + 300ms, "First line\nSecondLine" }, { 2s + 351ms, "ThirdLine" } });
            EXPECT_TRUE(dbLyrics->isSynchronized());
        }

        {
            auto transaction{ session.createReadTransaction() };

            const TrackLyrics::pointer dbLyrics{ lyrics.get() };

            EXPECT_TRUE(dbLyrics->isSynchronized());
            const auto synchronizedLines{ dbLyrics->getSynchronizedLines() };
            ASSERT_EQ(synchronizedLines.size(), 2);
            ASSERT_TRUE(synchronizedLines.contains(1s + 300ms));
            EXPECT_EQ(synchronizedLines.find(1s + 300ms)->second, "First line\nSecondLine");
            ASSERT_TRUE(synchronizedLines.contains(2s + 351ms));
            EXPECT_EQ(synchronizedLines.find(2s + 351ms)->second, "ThirdLine");
        }
    }

    TEST_F(DatabaseFixture, TrackLyrics_unsynchronized)
    {
        using namespace std::chrono_literals;

        ScopedTrack track{ session };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(TrackLyrics::getCount(session), 0);
        }

        ScopedTrackLyrics lyrics{ session };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(TrackLyrics::getCount(session), 1);
            EXPECT_EQ(TrackLyrics::getExternalLyricsCount(session), 0);

            const TrackLyrics::pointer dbLyrics{ lyrics.get() };
            EXPECT_FALSE(dbLyrics->isSynchronized());
            EXPECT_FALSE(dbLyrics->getLastWriteTime().isValid());
            EXPECT_TRUE(dbLyrics->getLastWriteTime().isNull());
        }

        {
            auto transaction{ session.createWriteTransaction() };

            TrackLyrics::pointer dbLyrics{ lyrics.get() };

            dbLyrics.modify()->setUnsynchronizedLines(std::array<std::string, 2>{ "First line\nSecondLine", "ThirdLine" });
            EXPECT_FALSE(dbLyrics->isSynchronized());
        }

        {
            auto transaction{ session.createReadTransaction() };

            const TrackLyrics::pointer dbLyrics{ lyrics.get() };

            EXPECT_FALSE(dbLyrics->isSynchronized());
            const std::vector<std::string> lines{ dbLyrics->getUnsynchronizedLines() };
            ASSERT_EQ(lines.size(), 2);
            EXPECT_EQ(lines[0], "First line\nSecondLine");
            EXPECT_EQ(lines[1], "ThirdLine");
        }
    }

    TEST_F(DatabaseFixture, TrackLyrics_external)
    {
        using namespace std::chrono_literals;

        ScopedTrack track{ session };
        ScopedTrackLyrics internalLyrics{ session };
        ScopedTrackLyrics externalLyrics{ session };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(TrackLyrics::getCount(session), 2);
            EXPECT_EQ(TrackLyrics::getExternalLyricsCount(session), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            externalLyrics.get().modify()->setAbsoluteFilePath("/tmp/test.lrc");
            externalLyrics.get().modify()->setTrack(track.get());
            internalLyrics.get().modify()->setTrack(track.get());
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(TrackLyrics::getCount(session), 2);
            EXPECT_EQ(TrackLyrics::getExternalLyricsCount(session), 1);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->clearEmbeddedLyrics();
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(TrackLyrics::getCount(session), 1);
            EXPECT_EQ(TrackLyrics::getExternalLyricsCount(session), 1);

            {
                bool visited{};
                TrackLyrics::find(session, TrackLyrics::FindParameters{}.setTrack(track.getId()), [&](const TrackLyrics::pointer& lyrics) {
                    EXPECT_EQ(lyrics->getAbsoluteFilePath(), "/tmp/test.lrc");
                    visited = true;
                });
                EXPECT_TRUE(visited);
            }

            {
                bool visited{};
                TrackLyrics::find(session, TrackLyrics::FindParameters{}.setExternal(true), [&](const TrackLyrics::pointer& lyrics) {
                    EXPECT_EQ(lyrics->getAbsoluteFilePath(), "/tmp/test.lrc");
                    visited = true;
                });
                EXPECT_TRUE(visited);
            }

            {
                bool visited{};
                TrackLyrics::find(session, TrackLyrics::FindParameters{}.setExternal(false), [&](const TrackLyrics::pointer&) {
                    visited = true;
                });
                EXPECT_FALSE(visited);
            }

            {
                bool visited{};
                TrackLyrics::find(session, TrackLyrics::FindParameters{}.setSortMethod(TrackLyricsSortMethod::EmbeddedFirst), [&](const TrackLyrics::pointer&) {
                    visited = true;
                });
                EXPECT_TRUE(visited);
            }

            {
                bool visited{};
                TrackLyrics::find(session, TrackLyrics::FindParameters{}.setSortMethod(TrackLyricsSortMethod::ExternalFirst), [&](const TrackLyrics::pointer&) {
                    visited = true;
                });
                EXPECT_TRUE(visited);
            }
        }
    }
} // namespace lms::db::tests