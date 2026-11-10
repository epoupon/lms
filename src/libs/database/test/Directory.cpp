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
#include "database/objects/Medium.hpp"

namespace lms::db::tests
{
    using ScopedDirectory = ScopedEntity<db::Directory>;
    using ScopedMedium = ScopedEntity<db::Medium>;

    TEST_F(DatabaseFixture, Directory)
    {
        ScopedDirectory directory{ session, "/path/to/dir/" };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(Directory::getCount(session), 1);

            std::optional<Directory> dir{ Directory::find(session, directory.getId()) };
            ASSERT_TRUE(dir);
            EXPECT_EQ(dir->getAbsolutePath(), "/path/to/dir");
            EXPECT_EQ(dir->getName(), "dir");
        }

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<Directory> dir{ Directory::find(session, directory.getId()) };
            ASSERT_TRUE(dir);
            dir->setAbsolutePath("/path/to/another/dir2");
            Directory::update(session, *dir);
        }

        {
            auto transaction{ session.createReadTransaction() };

            std::optional<Directory> dir{ Directory::find(session, directory.getId()) };
            ASSERT_TRUE(dir);
            EXPECT_EQ(dir->getAbsolutePath(), "/path/to/another/dir2");
            EXPECT_EQ(dir->getName(), "dir2");
        }

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<Directory> dir{ Directory::find(session, directory.getId()) };
            ASSERT_TRUE(dir);
            dir->setAbsolutePath("/foo/");
            Directory::update(session, *dir);
        }

        {
            auto transaction{ session.createReadTransaction() };

            std::optional<Directory> dir{ Directory::find(session, directory.getId()) };
            ASSERT_TRUE(dir);
            EXPECT_EQ(dir->getAbsolutePath(), "/foo");
            EXPECT_EQ(dir->getName(), "foo");
        }

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<Directory> dir{ Directory::find(session, directory.getId()) };
            ASSERT_TRUE(dir);
            dir->setAbsolutePath("/");
            Directory::update(session, *dir);
        }

        {
            auto transaction{ session.createReadTransaction() };

            std::optional<Directory> dir{ Directory::find(session, directory.getId()) };
            ASSERT_TRUE(dir);
            EXPECT_EQ(dir->getAbsolutePath(), "/");
            EXPECT_EQ(dir->getName(), "");
        }

        {
            auto transaction{ session.createReadTransaction() };

            std::optional<Directory> dir{ Directory::find(session, "/") };
            ASSERT_TRUE(dir);
            EXPECT_EQ(dir->getId(), directory.getId());
        }
    }

    TEST_F(DatabaseFixture, parent)
    {
        ScopedDirectory parent{ session, "/path/to/dir/" };
        ScopedDirectory child{ session, "/path/to/dir/child" };

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_FALSE(child->getParentDirectoryId().isValid());
        }

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<Directory> updatedChild{ child.get() };
            updatedChild->setParent(parent.getId());
            Directory::update(session, *updatedChild);
        }

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_EQ(child->getParentDirectoryId(), parent.getId());
        }

        {
            auto transaction{ session.createReadTransaction() };

            std::optional<Directory> foundDir;
            Directory::find(session, Directory::FindParameters{}.setParentDirectory(parent->getId()), [&](const Directory& dir) {
                ASSERT_FALSE(foundDir);
                foundDir = dir;
            });
            ASSERT_TRUE(foundDir);
            EXPECT_EQ(foundDir->getId(), child.getId());
        }
    }

    TEST_F(DatabaseFixture, Directory_orphaned)
    {
        ScopedDirectory parent{ session, "/path/to/dir/" };
        ScopedDirectory child{ session, "/path/to/dir/child" };

        {
            auto transaction{ session.createReadTransaction() };

            const auto directories{ Directory::findOrphanIds(session) };
            EXPECT_EQ(directories.size(), 2);
        }

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<Directory> updatedChild{ child.get() };
            updatedChild->setParent(parent.getId());
            Directory::update(session, *updatedChild);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto directories{ Directory::findOrphanIds(session) };
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

            std::optional<Directory> updatedChild{ child.get() };
            updatedChild->setParent(parent1.getId());
            Directory::update(session, *updatedChild);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto directories{ Directory::findRootDirectories(session) };
            ASSERT_EQ(directories.size(), 2);
            EXPECT_EQ(directories[0].getId(), parent1.getId());
            EXPECT_EQ(directories[1].getId(), parent2.getId());
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

            std::optional<Directory> updatedChild1{ child1.get() };
            updatedChild1->setParent(parent.getId());
            Directory::update(session, *updatedChild1);
            std::optional<Directory> updatedChild2{ child2.get() };
            updatedChild2->setParent(parent.getId());
            Directory::update(session, *updatedChild2);
        }

        {
            auto transaction{ session.createReadTransaction() };

            Directory::FindParameters params;
            params.setWithNoTrack(true);

            auto res{ Directory::find(session, params) };

            ASSERT_EQ(res.size(), 3);
            EXPECT_EQ(res[0].getId(), parent.getId());
            EXPECT_EQ(res[1].getId(), child1.getId());
            EXPECT_EQ(res[2].getId(), child2.getId());
        }

        {
            auto transaction{ session.createWriteTransaction() };
            std::optional<Track> updatedTrack{ track.get() };
            updatedTrack->setDirectory(child2.getId());
            Track::update(session, *updatedTrack);
        }

        {
            auto transaction{ session.createReadTransaction() };

            Directory::FindParameters params;
            params.setWithNoTrack(true);

            auto res{ Directory::find(session, params) };
            ASSERT_EQ(res.size(), 2);
            EXPECT_EQ(res[0].getId(), parent.getId());
            EXPECT_EQ(res[1].getId(), child1.getId());
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

            auto res{ Directory::find(session, params) };

            ASSERT_EQ(res.size(), 2);
            EXPECT_EQ(res[0].getId(), child1.getId());
            EXPECT_EQ(res[1].getId(), child2.getId());
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

            const auto res{ Directory::findMismatchedLibrary(session, std::nullopt, library->getPath(), library->getId()) };
            ASSERT_EQ(res.size(), 2);
            EXPECT_EQ(res[0], parent1.getId());
            EXPECT_EQ(res[1], child1.getId());
        }

        {
            auto transaction{ session.createWriteTransaction() };

            std::optional<Directory> updatedParent1{ parent1.get() };
            updatedParent1->setMediaLibrary(library.getId());
            Directory::update(session, *updatedParent1);
            std::optional<Directory> updatedChild1{ child1.get() };
            updatedChild1->setMediaLibrary(library.getId());
            Directory::update(session, *updatedChild1);
        }

        {
            auto transaction{ session.createReadTransaction() };

            const auto res{ Directory::findMismatchedLibrary(session, std::nullopt, library->getPath(), library->getId()) };
            EXPECT_EQ(res.size(), 0);
        }
    }

    TEST_F(DatabaseFixture, Directory_findByMedium)
    {
        ScopedDirectory dir1{ session, "/root" };
        ScopedDirectory dir2{ session, "/root" };

        ScopedTrack track1{ session };
        ScopedTrack track2{ session };

        ScopedRelease release1{ session, "Release1" };
        ScopedMedium medium1{ session, release1.getId() };

        ScopedRelease release2{ session, "Release2" };
        ScopedMedium medium2{ session, release1.getId() };

        {
            auto transaction{ session.createReadTransaction() };

            Directory::FindParameters params;
            params.setMedium(medium1.getId());

            bool visited{};
            Directory::find(session, params, [&](const Directory&) {
                visited = true;
            });
            EXPECT_FALSE(visited);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            std::optional<Track> updatedTrack1{ track1.get() };
            updatedTrack1->setMedium(medium1.getId());
            updatedTrack1->setRelease(release1.getId());
            updatedTrack1->setDirectory(dir1.getId());
            Track::update(session, *updatedTrack1);

            std::optional<Track> updatedTrack2{ track2.get() };
            updatedTrack2->setMedium(medium2.getId());
            updatedTrack2->setRelease(release2.getId());
            updatedTrack2->setDirectory(dir2.getId());
            Track::update(session, *updatedTrack2);
        }

        {
            auto transaction{ session.createReadTransaction() };

            Directory::FindParameters params;
            params.setMedium(medium1.getId());

            std::vector<DirectoryId> visitedDirectories;
            Directory::find(session, params, [&](const Directory& dir) {
                visitedDirectories.push_back(dir.getId());
            });
            ASSERT_EQ(visitedDirectories.size(), 1);
            EXPECT_EQ(visitedDirectories[0], dir1.getId());
        }
    }
} // namespace lms::db::tests