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

#include "Common.hpp"

#include "database/objects/PlayListFile.hpp"
#include "database/objects/TrackList.hpp"

namespace lms::db::tests
{
    using ScopedPlayListFile = ScopedEntity<db::PlayListFile>;

    TEST_F(DatabaseFixture, PlayListFile)
    {
        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(PlayListFile::getCount(session), 0);
        }

        ScopedPlayListFile playlist{ session, "/tmp/foo.m3u" };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(PlayListFile::getCount(session), 1);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const PlayListFile::pointer dbPlayList{ playlist.get() };

            EXPECT_EQ(dbPlayList->getAbsoluteFilePath(), "/tmp/foo.m3u");
            EXPECT_EQ(dbPlayList->getLastWriteTime(), Wt::WDateTime{});
            EXPECT_EQ(dbPlayList->getFileSize(), 0);
            EXPECT_EQ(dbPlayList->getName(), "");
            EXPECT_EQ(dbPlayList->getTrackList(), TrackList::pointer{});
        }

        ScopedTrackList trackList{ session, "MyTrackList", TrackListType::PlayList };

        // Now change some values
        {
            auto transaction{ session.createWriteTransaction() };
            PlayListFile::pointer dbPlayList{ playlist.get() };

            dbPlayList.modify()->setAbsoluteFilePath("/tmp/bar.m3u");
            dbPlayList.modify()->setLastWriteTime(Wt::WDateTime{ Wt::WDate{ 2024, 30, 1 }, Wt::WTime{ 12, 58, 29 } });
            dbPlayList.modify()->setFileSize(1234);
            dbPlayList.modify()->setName("My playlist");
            dbPlayList.modify()->setTrackList(trackList.get());
            dbPlayList.modify()->setFiles({ { std::filesystem::path{ "/foo/foo.mp3" }, std::filesystem::path{ "/foo/bar.mp3" } } });
        }

        // Check values are reflected
        {
            auto transaction{ session.createReadTransaction() };

            const PlayListFile::pointer dbPlayList{ playlist.get() };

            EXPECT_EQ(dbPlayList->getAbsoluteFilePath(), "/tmp/bar.m3u");
            EXPECT_EQ(dbPlayList->getLastWriteTime(), (Wt::WDateTime{ Wt::WDate{ 2024, 30, 1 }, Wt::WTime{ 12, 58, 29 } }));
            EXPECT_EQ(dbPlayList->getFileSize(), 1234);
            EXPECT_EQ(dbPlayList->getName(), "My playlist");
            EXPECT_EQ(dbPlayList->getTrackList(), trackList.get());
            const auto files{ dbPlayList->getFiles() };
            ASSERT_EQ(files.size(), 2);
            EXPECT_EQ(files[0], "/foo/foo.mp3");
            EXPECT_EQ(files[1], "/foo/bar.mp3");
        }
    }

    TEST_F(DatabaseFixture, PlayListFile_findAbsoluteFilePath)
    {
        ScopedPlayListFile playlist{ session, "/tmp/foo.m3u" };

        {
            auto transaction{ session.createReadTransaction() };

            PlayListFileId lastRetrievedId;
            std::filesystem::path retrievedFilePath;
            PlayListFile::findAbsoluteFilePath(session, lastRetrievedId, 1, [&](PlayListFileId playListFileId, const std::filesystem::path& filePath) {
                EXPECT_EQ(playListFileId, playlist.getId());
                retrievedFilePath = filePath;
            });

            EXPECT_EQ(retrievedFilePath, "/tmp/foo.m3u");
        }
    }

    TEST_F(DatabaseFixture, PlayListFile_deleteTrackList)
    {
        {
            ScopedPlayListFile playlist{ session, "/tmp/foo.m3u" };

            {
                auto transaction{ session.createWriteTransaction() };
                TrackList::pointer trackList{ session.create<TrackList>("MyTrackList", TrackListType::PlayList) };
                playlist.get().modify()->setTrackList(trackList);
            }
            {
                auto transaction{ session.createReadTransaction() };
                ASSERT_EQ(TrackList::getCount(session), 1);
            }
        }

        {
            auto transaction{ session.createReadTransaction() };
            ASSERT_EQ(PlayListFile::getCount(session), 0);
        }

        {
            auto transaction{ session.createReadTransaction() };
            ASSERT_EQ(TrackList::getCount(session), 0);
        }
    }
} // namespace lms::db::tests