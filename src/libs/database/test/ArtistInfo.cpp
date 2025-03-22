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

#include "database/Artist.hpp"
#include "database/ArtistInfo.hpp"
#include "database/Directory.hpp"

namespace lms::db::tests
{
    using ScopedArtistInfo = ScopedEntity<db::ArtistInfo>;
    using ScopedDirectory = ScopedEntity<db::Directory>;

    TEST_F(DatabaseFixture, ArtistInfo)
    {
        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(ArtistInfo::getCount(session), 0);
        }

        ScopedArtistInfo artistInfo{ session };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(ArtistInfo::getCount(session), 1);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const ArtistInfo::pointer dbArtistInfo{ ArtistInfo::find(session, artistInfo.getId()) };

            EXPECT_EQ(dbArtistInfo->getAbsoluteFilePath(), "");
            EXPECT_EQ(dbArtistInfo->getLastWriteTime(), Wt::WDateTime{});

            EXPECT_EQ(dbArtistInfo->getArtist(), Artist::pointer{});
            EXPECT_EQ(dbArtistInfo->getDirectory(), Directory::pointer{});
            EXPECT_EQ(dbArtistInfo->getType(), "");
            EXPECT_EQ(dbArtistInfo->getGender(), "");
            EXPECT_EQ(dbArtistInfo->getDisambiguation(), "");
            EXPECT_EQ(dbArtistInfo->getBiography(), "");
        }

        ScopedArtist artist{ session, "MyArtist" };
        ScopedDirectory directory{ session, "/tmp" };

        const Wt::WDateTime dateTime{ Wt::WDate{ 2024, 30, 1 }, Wt::WTime{ 12, 58, 29 } };

        // Now change some values
        {
            auto transaction{ session.createWriteTransaction() };
            ArtistInfo::pointer dbArtistInfo{ ArtistInfo::find(session, artistInfo.getId()) };

            dbArtistInfo.modify()->setAbsoluteFilePath("/tmp/artist.nfo");
            dbArtistInfo.modify()->setLastWriteTime(dateTime);

            dbArtistInfo.modify()->setArtist(artist.get());
            dbArtistInfo.modify()->setDirectory(directory.get());
            dbArtistInfo.modify()->setType("MyType");
            dbArtistInfo.modify()->setGender("MyGender");
            dbArtistInfo.modify()->setDisambiguation("MyDisambiguation");
            dbArtistInfo.modify()->setBiography("MyBiography");
        }

        // Check values are reflected
        {
            auto transaction{ session.createReadTransaction() };

            const ArtistInfo::pointer dbArtistInfo{ ArtistInfo::find(session, artistInfo.getId()) };

            EXPECT_EQ(dbArtistInfo->getAbsoluteFilePath(), "/tmp/artist.nfo");
            EXPECT_EQ(dbArtistInfo->getLastWriteTime(), dateTime);

            EXPECT_EQ(dbArtistInfo->getArtist(), artist.get());
            EXPECT_EQ(dbArtistInfo->getDirectory(), directory.get());
            EXPECT_EQ(dbArtistInfo->getDirectoryId(), directory.getId());
            EXPECT_EQ(dbArtistInfo->getType(), "MyType");
            EXPECT_EQ(dbArtistInfo->getGender(), "MyGender");
            EXPECT_EQ(dbArtistInfo->getDisambiguation(), "MyDisambiguation");
            EXPECT_EQ(dbArtistInfo->getBiography(), "MyBiography");
        }

        {
            auto transaction{ session.createReadTransaction() };

            bool visited{};
            ArtistInfo::find(session, artist.getId(), [&](const ArtistInfo::pointer& dbArtistInfo) {
                ASSERT_NE(dbArtistInfo, ArtistInfo::pointer{});
                EXPECT_EQ(dbArtistInfo->getId(), artistInfo.getId());
                visited = true;
            });
            EXPECT_TRUE(visited);
        }
    }
} // namespace lms::db::tests