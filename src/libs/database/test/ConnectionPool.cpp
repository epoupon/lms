/*
 * Copyright (C) 2026 Emeric Poupon
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

#include <gtest/gtest.h>

#include <Wt/Dbo/SqlConnection.h>
#include <Wt/Dbo/Transaction.h>

#include "database/Session.hpp"

#include "TmpDatabase.hpp"

namespace lms::db::tests
{
    namespace
    {
        class ConnectionPoolTest : public ::testing::Test
        {
        protected:
            static Wt::Dbo::SqlConnection* getConnectionUsedBy(Session& usedSession)
            {
                const Wt::Dbo::Transaction transaction{ *usedSession.getDboSession() };
                return transaction.connection();
            }

            TmpDatabase tmpDb;
            Session session{ tmpDb.getDb() };
            Session otherSession{ tmpDb.getDb() };
        };
    } // namespace

    TEST_F(ConnectionPoolTest, freeConnectionIsReused)
    {
        Wt::Dbo::SqlConnection* connection{ getConnectionUsedBy(session) };

        EXPECT_EQ(getConnectionUsedBy(session), connection);
        EXPECT_EQ(getConnectionUsedBy(otherSession), connection);
    }

    TEST_F(ConnectionPoolTest, newConnectionIsOpenedWhenNoneIsFree)
    {
        const Wt::Dbo::Transaction transaction{ *session.getDboSession() };
        const Wt::Dbo::Transaction otherTransaction{ *otherSession.getDboSession() };

        EXPECT_NE(transaction.connection(), otherTransaction.connection());
    }

    TEST_F(ConnectionPoolTest, connectionIsReplacedOnceReloadIsRequested)
    {
        Wt::Dbo::SqlConnection* connection{ getConnectionUsedBy(session) };

        session.requestConnectionsReload();

        Wt::Dbo::SqlConnection* newConnection{ getConnectionUsedBy(session) };
        EXPECT_NE(newConnection, connection);
        EXPECT_EQ(getConnectionUsedBy(session), newConnection);
    }
} // namespace lms::db::tests
