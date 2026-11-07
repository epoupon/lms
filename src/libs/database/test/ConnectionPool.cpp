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

#include <memory>

#include <gtest/gtest.h>

#include <Wt/Dbo/SqlConnection.h>
#include <Wt/Dbo/Transaction.h>

#include "database/IDb.hpp"

#include "Session.hpp"
#include "TmpDatabase.hpp"

namespace lms::db::tests
{
    namespace
    {
        class ConnectionPoolTest : public ::testing::Test
        {
        protected:
            static Wt::Dbo::SqlConnection* getConnectionUsedBy(ISession& usedSession)
            {
                const Wt::Dbo::Transaction transaction{ *getDboSession(usedSession) };
                return transaction.connection();
            }

            TmpDatabase tmpDb;
            const std::unique_ptr<ISession> session{ tmpDb.getDb().createSession() };
            const std::unique_ptr<ISession> otherSession{ tmpDb.getDb().createSession() };
        };
    } // namespace

    TEST_F(ConnectionPoolTest, freeConnectionIsReused)
    {
        Wt::Dbo::SqlConnection* connection{ getConnectionUsedBy(*session) };

        EXPECT_EQ(getConnectionUsedBy(*session), connection);
        EXPECT_EQ(getConnectionUsedBy(*otherSession), connection);
    }

    TEST_F(ConnectionPoolTest, newConnectionIsOpenedWhenNoneIsFree)
    {
        const Wt::Dbo::Transaction transaction{ *getDboSession(*session) };
        const Wt::Dbo::Transaction otherTransaction{ *getDboSession(*otherSession) };

        EXPECT_NE(transaction.connection(), otherTransaction.connection());
    }

    TEST_F(ConnectionPoolTest, connectionIsReplacedAfterAnalyze)
    {
        Wt::Dbo::SqlConnection* connection{ getConnectionUsedBy(*session) };

        tmpDb.getDb().analyze();

        Wt::Dbo::SqlConnection* newConnection{ getConnectionUsedBy(*session) };
        EXPECT_NE(newConnection, connection);
        EXPECT_EQ(getConnectionUsedBy(*session), newConnection);
    }

    TEST_F(ConnectionPoolTest, connectionIsReplacedAfterAbortedAnalyze)
    {
        tmpDb.createTablesAndIndexes();

        Wt::Dbo::SqlConnection* connection{ getConnectionUsedBy(*session) };

        bool abortRequested{};
        tmpDb.getDb().analyze({}, [&] {
            abortRequested = true;
            return true;
        });

        EXPECT_TRUE(abortRequested);
        EXPECT_NE(getConnectionUsedBy(*session), connection);
    }
} // namespace lms::db::tests
