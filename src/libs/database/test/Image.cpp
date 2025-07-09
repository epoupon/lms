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

#include "database/objects/Directory.hpp"
#include "database/objects/Image.hpp"

namespace lms::db::tests
{
    using ScopedDirectory = ScopedEntity<db::Directory>;
    using ScopedImage = ScopedEntity<db::Image>;

    TEST_F(DatabaseFixture, Image)
    {
        ScopedImage image{ session, "/path/to/image" };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Image::getCount(session), 1);

            Image::pointer img{ Image::find(session, image.getId()) };
            ASSERT_NE(img, Image::pointer{});
            EXPECT_EQ(img->getAbsoluteFilePath(), "/path/to/image");
            EXPECT_EQ(img->getFileStem(), "image");
            EXPECT_EQ(img->getWidth(), 0);
            EXPECT_EQ(img->getHeight(), 0);
            EXPECT_EQ(img->getFileSize(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            Image::pointer img{ Image::find(session, image.getId()) };
            ASSERT_NE(img, Image::pointer{});
            img.modify()->setAbsoluteFilePath("/path/to/another/image2");
            img.modify()->setWidth(640);
            img.modify()->setHeight(480);
            img.modify()->setFileSize(1024 * 1024);
        }

        {
            auto transaction{ session.createReadTransaction() };

            Image::pointer img{ Image::find(session, image.getId()) };
            ASSERT_NE(img, Image::pointer{});
            EXPECT_EQ(img->getAbsoluteFilePath(), "/path/to/another/image2");
            EXPECT_EQ(img->getFileStem(), "image2");
            EXPECT_EQ(img->getWidth(), 640);
            EXPECT_EQ(img->getHeight(), 480);
            EXPECT_EQ(img->getFileSize(), 1024 * 1024);
        }

        {
            auto transaction{ session.createReadTransaction() };

            Image::pointer img{ Image::find(session, "/path/to/another/image2") };
            ASSERT_NE(img, Image::pointer{});
            EXPECT_EQ(img->getId(), image->getId());
        }
    }

    TEST_F(DatabaseFixture, Image_inDirectory)
    {
        ScopedImage image{ session, "/path/to/image" };
        ScopedDirectory directory{ session, "/path/to" };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Image::find(session, Image::FindParameters{}.setDirectory(directory.getId())).results.size(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            image.get().modify()->setDirectory(directory.get());
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto results{ Image::find(session, Image::FindParameters{}.setDirectory(directory.getId())).results };
            ASSERT_EQ(results.size(), 1);
            EXPECT_EQ(results.front()->getId(), image.getId());
        }
    }

    TEST_F(DatabaseFixture, Image_findAbsoluteFilePath)
    {
        ScopedImage image{ session, "/path/to/image" };

        const std::filesystem::path absoluteFilePath{ "/path/to/image" };
        {
            auto transaction{ session.createWriteTransaction() };
            image.get().modify()->setAbsoluteFilePath(absoluteFilePath);
        }

        {
            auto transaction{ session.createReadTransaction() };
            ImageId lastRetrievedImageId;
            std::filesystem::path retrievedPath;
            Image::findAbsoluteFilePath(session, lastRetrievedImageId, 1, [&](ImageId id, const std::filesystem::path& path) {
                EXPECT_EQ(id, image.getId());
                retrievedPath = path;
            });

            EXPECT_EQ(retrievedPath, absoluteFilePath);
        }
    }

} // namespace lms::db::tests