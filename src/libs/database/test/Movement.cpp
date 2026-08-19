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

#include "database/objects/Movement.hpp"
#include "database/objects/Track.hpp"

namespace lms::db::tests
{
    TEST_F(DatabaseFixture, Movement_create)
    {
        ScopedTrack track{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            Movement::create(session, "Allegro con brio", std::size_t{ 1 }, std::size_t{ 4 }, track.get());
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto movements{ track->getMovements() };
            ASSERT_EQ(movements.size(), 1);
            ASSERT_FALSE(movements[0]->getName().empty());
            EXPECT_EQ(movements[0]->getName(), "Allegro con brio");
            ASSERT_TRUE(movements[0]->getNumber());
            EXPECT_EQ(*movements[0]->getNumber(), std::size_t{ 1 });
            ASSERT_TRUE(movements[0]->getCount());
            EXPECT_EQ(*movements[0]->getCount(), std::size_t{ 4 });
        }
    }

    TEST_F(DatabaseFixture, Movement_createWithNullFields)
    {
        ScopedTrack track{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            Movement::create(session, "", std::nullopt, std::nullopt, track.get());
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto movements{ track->getMovements() };
            ASSERT_EQ(movements.size(), 1);
            EXPECT_TRUE(movements[0]->getName().empty());
            EXPECT_FALSE(movements[0]->getNumber());
            EXPECT_FALSE(movements[0]->getCount());
        }
    }

    TEST_F(DatabaseFixture, Movement_multipleMovementsOnTrack)
    {
        ScopedTrack track{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            Movement::create(session, "Allegro con brio", std::size_t{ 1 }, std::size_t{ 4 }, track.get());
            Movement::create(session, "Andante con moto", std::size_t{ 2 }, std::size_t{ 4 }, track.get());
            Movement::create(session, "Scherzo. Allegro", std::size_t{ 3 }, std::size_t{ 4 }, track.get());
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto movements{ track->getMovements() };
            EXPECT_EQ(movements.size(), 3);
        }
    }

    TEST_F(DatabaseFixture, Movement_cascadeDeleteWithTrack)
    {
        ScopedTrack track{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            Movement::create(session, "Allegro", std::size_t{ 1 }, std::nullopt, track.get());
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(track->getMovements().size(), 1);
        }

        // track goes out of scope here — movements are cascade deleted
    }

    TEST_F(DatabaseFixture, Movement_clearMovements)
    {
        ScopedTrack track{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            Movement::create(session, "Allegro", std::size_t{ 1 }, std::size_t{ 2 }, track.get());
            Movement::create(session, "Andante", std::size_t{ 2 }, std::size_t{ 2 }, track.get());
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(track->getMovements().size(), 2);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->clearMovements();
        }

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(track->getMovements().size(), 0);
        }
    }

    TEST_F(DatabaseFixture, Movement_replacingMovements)
    {
        ScopedTrack track{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            Movement::create(session, "OldMovement", std::size_t{ 1 }, std::nullopt, track.get());
        }

        {
            auto transaction{ session.createWriteTransaction() };
            track.get().modify()->clearMovements();
            Movement::create(session, "NewMovement", std::size_t{ 1 }, std::nullopt, track.get());
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto movements{ track->getMovements() };
            ASSERT_EQ(movements.size(), 1);
            ASSERT_FALSE(movements[0]->getName().empty());
            EXPECT_EQ(movements[0]->getName(), "NewMovement");
        }
    }

    TEST_F(DatabaseFixture, Movement_getNumberReturnsCorrectSizeT)
    {
        ScopedTrack track{ session };

        {
            auto transaction{ session.createWriteTransaction() };
            Movement::create(session, "", std::size_t{ 42 }, std::size_t{ 100 }, track.get());
        }

        {
            auto transaction{ session.createReadTransaction() };
            const auto movements{ track->getMovements() };
            ASSERT_EQ(movements.size(), 1);
            ASSERT_TRUE(movements[0]->getNumber());
            EXPECT_EQ(*movements[0]->getNumber(), std::size_t{ 42 });
            ASSERT_TRUE(movements[0]->getCount());
            EXPECT_EQ(*movements[0]->getCount(), std::size_t{ 100 });
        }
    }
} // namespace lms::db::tests
