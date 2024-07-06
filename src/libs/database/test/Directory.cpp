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

#include "database/Directory.hpp"

namespace lms::db::tests
{
    using ScopedDirectory = ScopedEntity<db::Directory>;

    TEST_F(DatabaseFixture, Directory)
    {
        ScopedDirectory directory{ session, "/path/to/dir/" };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Directory::getCount(session), 1);

            Directory::pointer dir{ Directory::find(session, directory.getId()) };
            ASSERT_NE(dir, Directory::pointer{});
            EXPECT_EQ(dir->getAbsolutePath(), "/path/to/dir");
            EXPECT_EQ(dir->getName(), "dir");
        }

        {
            auto transaction{ session.createWriteTransaction() };

            Directory::pointer dir{ Directory::find(session, directory.getId()) };
            ASSERT_NE(dir, Directory::pointer{});
            dir.modify()->setAbsolutePath("/path/to/another/dir2");
        }

        {
            auto transaction{ session.createReadTransaction() };

            Directory::pointer dir{ Directory::find(session, directory.getId()) };
            ASSERT_NE(dir, Directory::pointer{});
            EXPECT_EQ(dir->getAbsolutePath(), "/path/to/another/dir2");
            EXPECT_EQ(dir->getName(), "dir2");
        }

        {
            auto transaction{ session.createWriteTransaction() };

            Directory::pointer dir{ Directory::find(session, directory.getId()) };
            ASSERT_NE(dir, Directory::pointer{});
            dir.modify()->setAbsolutePath("/foo/");
        }

        {
            auto transaction{ session.createReadTransaction() };

            Directory::pointer dir{ Directory::find(session, directory.getId()) };
            ASSERT_NE(dir, Directory::pointer{});
            EXPECT_EQ(dir->getAbsolutePath(), "/foo");
            EXPECT_EQ(dir->getName(), "foo");
        }

        {
            auto transaction{ session.createWriteTransaction() };

            Directory::pointer dir{ Directory::find(session, directory.getId()) };
            ASSERT_NE(dir, Directory::pointer{});
            dir.modify()->setAbsolutePath("/");
        }

        {
            auto transaction{ session.createReadTransaction() };

            Directory::pointer dir{ Directory::find(session, directory.getId()) };
            ASSERT_NE(dir, Directory::pointer{});
            EXPECT_EQ(dir->getAbsolutePath(), "/");
            EXPECT_EQ(dir->getName(), "");
        }

        {
            auto transaction{ session.createReadTransaction() };

            Directory::pointer dir{ Directory::find(session, "/") };
            ASSERT_NE(dir, Directory::pointer{});
            EXPECT_EQ(dir->getId(), directory.getId());
        }
    }

    TEST_F(DatabaseFixture, parent)
    {
        ScopedDirectory parent{ session, "/path/to/dir/" };
        ScopedDirectory child{ session, "/path/to/dir/child" };

        {
            auto transaction{ session.createReadTransaction() };

            auto dir{ child->getParent() };
            EXPECT_EQ(dir, Directory::pointer{});
        }

        {
            auto transaction{ session.createWriteTransaction() };

            child.get().modify()->setParent(parent.lockAndGet());
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto dir{ child->getParent() };
            ASSERT_NE(dir, Directory::pointer{});
            EXPECT_EQ(dir->getId(), parent.getId());
        }
    }

    TEST_F(DatabaseFixture, Directory_orphaned)
    {
        ScopedDirectory parent{ session, "/path/to/dir/" };
        ScopedDirectory child{ session, "/path/to/dir/child" };

        {
            auto transaction{ session.createReadTransaction() };

            const auto directories{ Directory::findOrphanIds(session).results };
            EXPECT_EQ(directories.size(), 2);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            child.get().modify()->setParent(parent.lockAndGet());
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto directories{ Directory::findOrphanIds(session).results };
            ASSERT_EQ(directories.size(), 1);
            EXPECT_EQ(directories.front(), child.getId());
        }
    }
} // namespace lms::db::tests