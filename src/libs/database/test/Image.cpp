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

#include "database/Image.hpp"

namespace lms::db::tests
{
    using ScopedImage = ScopedEntity<db::Image>;

    TEST_F(DatabaseFixture, Image)
    {
        ScopedImage image{ session, "/path/to/image" };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Image::getCount(session), 1);

            Image::pointer img{ Image::find(session, image.getId()) };
            ASSERT_NE(img, Image::pointer{});
            EXPECT_EQ(img->getPath(), "/path/to/image");
            EXPECT_EQ(img->getWidth(), 0);
            EXPECT_EQ(img->getHeight(), 0);
            EXPECT_EQ(img->getFileSize(), 0);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            Image::pointer img{ Image::find(session, image.getId()) };
            ASSERT_NE(img, Image::pointer{});
            img.modify()->setPath("/path/to/another/image");
            img.modify()->setWidth(640);
            img.modify()->setHeight(480);
            img.modify()->setFileSize(1024 * 1024);
        }

        {
            auto transaction{ session.createReadTransaction() };

            Image::pointer img{ Image::find(session, image.getId()) };
            ASSERT_NE(img, Image::pointer{});
            EXPECT_EQ(img->getPath(), "/path/to/another/image");
            EXPECT_EQ(img->getWidth(), 640);
            EXPECT_EQ(img->getHeight(), 480);
            EXPECT_EQ(img->getFileSize(), 1024 * 1024);
        }
    }
} // namespace lms::db::tests