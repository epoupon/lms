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

            auto dir{ child->getParentDirectory() };
            EXPECT_EQ(dir, Directory::pointer{});
        }

        {
            auto transaction{ session.createWriteTransaction() };

            child.get().modify()->setParent(parent.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            auto dir{ child->getParentDirectory() };
            ASSERT_NE(dir, Directory::pointer{});
            EXPECT_EQ(dir->getId(), parent.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            Directory::pointer foundDir;
            Directory::find(session, Directory::FindParameters{}.setParentDirectory(parent->getId()), [&](const Directory::pointer& dir) {
                ASSERT_EQ(foundDir, Directory::pointer{});
                foundDir = dir;
            });
            ASSERT_NE(foundDir, Directory::pointer{});
            EXPECT_EQ(foundDir->getId(), child.getId());
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

            child.get().modify()->setParent(parent.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto directories{ Directory::findOrphanIds(session).results };
            ASSERT_EQ(directories.size(), 1);
            EXPECT_EQ(directories.front(), child.getId());
        }
    }

    TEST_F(DatabaseFixture, Directory_findRootDirectories)
    {
        ScopedDirectory parent1{ session, "/root1" };
        ScopedDirectory child{ session, "/root1/child" };
        ScopedDirectory parent2{ session, "/root2" };

        {
            auto transaction{ session.createWriteTransaction() };

            child.get().modify()->setParent(parent1.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto directories{ Directory::findRootDirectories(session).results };
            ASSERT_EQ(directories.size(), 2);
            EXPECT_EQ(directories[0]->getId(), parent1.getId());
            EXPECT_EQ(directories[1]->getId(), parent2.getId());
        }
    }

    TEST_F(DatabaseFixture, Directory_findNonTrackDirectories)
    {
        ScopedDirectory parent{ session, "/root" };
        ScopedDirectory child1{ session, "/root/child1" };
        ScopedDirectory child2{ session, "/root/child2" };
        ScopedTrack track{ session };

        {
            auto transaction{ session.createWriteTransaction() };

            child1.get().modify()->setParent(parent.get());
            child2.get().modify()->setParent(parent.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            Directory::FindParameters params;
            params.setWithNoTrack(true);

            auto res{ Directory::find(session, params).results };

            ASSERT_EQ(res.size(), 3);
            EXPECT_EQ(res[0]->getId(), parent.getId());
            EXPECT_EQ(res[1]->getId(), child1.getId());
            EXPECT_EQ(res[2]->getId(), child2.getId());
        }

        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->setDirectory(child2.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            Directory::FindParameters params;
            params.setWithNoTrack(true);

            auto res{ Directory::find(session, params).results };
            ASSERT_EQ(res.size(), 2);
            EXPECT_EQ(res[0]->getId(), parent.getId());
            EXPECT_EQ(res[1]->getId(), child1.getId());
        }
    }

    TEST_F(DatabaseFixture, Directory_findWithKeywords)
    {
        ScopedDirectory parent{ session, "/root" };
        ScopedDirectory child1{ session, "/root/foo" };
        ScopedDirectory child2{ session, "/root/bar/foo" };

        {
            auto transaction{ session.createReadTransaction() };

            Directory::FindParameters params;
            params.setKeywords({ "foo" });

            auto res{ Directory::find(session, params).results };

            ASSERT_EQ(res.size(), 2);
            EXPECT_EQ(res[0]->getId(), child1.getId());
            EXPECT_EQ(res[1]->getId(), child2.getId());
        }
    }

    TEST_F(DatabaseFixture, Directory_findMismatchedLibrary)
    {
        ScopedDirectory parent1{ session, "/root" };
        ScopedDirectory child1{ session, "/root/foo" };
        ScopedDirectory parent2{ session, "/root_1" };
        ScopedDirectory child2{ session, "/root_1/foo" };

        ScopedMediaLibrary library{ session, "MyLibrary", "/root" };

        {
            auto transaction{ session.createReadTransaction() };

            const auto res{ Directory::findMismatchedLibrary(session, std::nullopt, library->getPath(), library->getId()).results };
            ASSERT_EQ(res.size(), 2);
            EXPECT_EQ(res[0], parent1.getId());
            EXPECT_EQ(res[1], child1.getId());
        }

        {
            auto transaction{ session.createWriteTransaction() };

            parent1.get().modify()->setMediaLibrary(library.get());
            child1.get().modify()->setMediaLibrary(library.get());
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto res{ Directory::findMismatchedLibrary(session, std::nullopt, library->getPath(), library->getId()).results };
            EXPECT_EQ(res.size(), 0);
        }
    }
} // namespace lms::db::tests