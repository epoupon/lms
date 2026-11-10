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

#include "Common.hpp"

#include "database/objects/Artwork.hpp"
#include "database/objects/Directory.hpp"
#include "database/objects/Podcast.hpp"

namespace lms::db::tests
{
    using ScopedDirectory = ScopedEntity<db::Directory>;
    using ScopedPodcast = ScopedEntity<db::Podcast>;

    TEST_F(DatabaseFixture, Podcast)
    {
        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Podcast::getCount(session), 0);
        }

        ScopedPodcast podcast{ session, "podcastUrl" };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Podcast::getCount(session), 1);

            std::optional<Podcast> p{ Podcast::find(session, podcast.getId()) };
            ASSERT_TRUE(p);
            EXPECT_EQ(p->getUrl(), "podcastUrl");
            EXPECT_EQ(p->getTitle(), "");
            EXPECT_EQ(p->getLink(), "");
            EXPECT_EQ(p->getDescription(), "");
            EXPECT_EQ(p->getLanguage(), "");
            EXPECT_EQ(p->getCopyright(), "");
            EXPECT_EQ(p->getLastBuildDate(), Wt::WDateTime());
            EXPECT_EQ(p->getAuthor(), "");
            EXPECT_EQ(p->getCategory(), "");
            EXPECT_EQ(p->isExplicit(), false);
            EXPECT_EQ(p->getImageUrl(), "");
            EXPECT_EQ(p->getOwnerEmail(), "");
        }

        {
            auto transaction{ session.createWriteTransaction() };
            std::optional<Podcast> p{ Podcast::find(session, podcast.getId()) };
            ASSERT_TRUE(p);
            p->setUrl("newPodcastUrl");
            p->setTitle("newTitle");
            p->setLink("newLink");
            p->setDescription("newDescription");
            p->setLanguage("newLanguage");
            p->setCopyright("newCopyright");
            p->setLastBuildDate(Wt::WDateTime::currentDateTime());
            p->setAuthor("newAuthor");
            p->setCategory("newCategory");
            p->setExplicit(true);
            p->setImageUrl("newImageUrl");
            p->setOwnerEmail("newOwnerEmail");
            p->setOwnerName("newOwnerName");
            Podcast::update(session, *p);
        }

        {
            auto transaction{ session.createReadTransaction() };

            std::optional<Podcast> img{ Podcast::find(session, podcast.getId()) };
            ASSERT_TRUE(img);
            EXPECT_EQ(img->getUrl(), "newPodcastUrl");
            EXPECT_EQ(img->getTitle(), "newTitle");
            EXPECT_EQ(img->getLink(), "newLink");
            EXPECT_EQ(img->getDescription(), "newDescription");
            EXPECT_EQ(img->getLanguage(), "newLanguage");
            EXPECT_EQ(img->getCopyright(), "newCopyright");
            EXPECT_TRUE(img->getLastBuildDate().isValid());
            EXPECT_EQ(img->getAuthor(), "newAuthor");
            EXPECT_EQ(img->getCategory(), "newCategory");
            EXPECT_TRUE(img->isExplicit());
            EXPECT_EQ(img->getImageUrl(), "newImageUrl");
            EXPECT_EQ(img->getOwnerEmail(), "newOwnerEmail");
            EXPECT_EQ(img->getOwnerName(), "newOwnerName");
        }
    }

} // namespace lms::db::tests