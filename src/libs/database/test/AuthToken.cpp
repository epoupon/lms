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

#include "database/objects/AuthToken.hpp"

namespace lms::db::tests
{
    using ScopedAuthToken = ScopedEntity<db::AuthToken>;

    TEST_F(DatabaseFixture, AuthTokens)
    {
        ScopedUser user{ session, "MyUser" };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(AuthToken::getCount(session), 0);
        }

        ScopedAuthToken token{ session, "myDomain", "foo", Wt::WDateTime{}, std::nullopt, user.lockAndGet() };

        {
            auto transaction{ session.createReadTransaction() };
            EXPECT_EQ(AuthToken::getCount(session), 1);
        }

        {
            auto transaction{ session.createWriteTransaction() };
            AuthToken::clearUserTokens(session, "nonExistingDomain", user.getId());
        }
    }
} // namespace lms::db::tests