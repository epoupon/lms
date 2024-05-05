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

namespace lms::db::tests
{
    TEST_F(DatabaseFixture, User)
    {
        {
            auto transaction{ session.createReadTransaction() };

            bool visited{};
            User::find(session, User::FindParameters{}, [&](const User::pointer&)
                {
                    visited = true;
                });
            EXPECT_FALSE(visited);
        }

        ScopedUser user1{ session, "MyUser1" };
        ScopedUser user2{ session, "MyUser2" };

        {
            auto transaction{ session.createReadTransaction() };

            std::vector<UserId> visitedUsers;
            User::find(session, User::FindParameters{}, [&](const User::pointer& user)
                {
                    visitedUsers.push_back(user->getId());
                });
            EXPECT_EQ(visitedUsers.size(), 2);
            EXPECT_EQ(visitedUsers[0], user1->getId());
            EXPECT_EQ(visitedUsers[1], user2->getId());
        }
    }
}