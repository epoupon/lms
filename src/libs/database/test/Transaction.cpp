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

#include <array>
#include <stdexcept>
#include <string_view>

#include <Wt/Dbo/Exception.h>
#include <Wt/Dbo/Impl.h>

#include "Common.hpp"
#include "Session.hpp"
#include "Utils.hpp"

namespace lms::db::tests
{
    namespace
    {
        // SQLite only reports a deferred foreign key violation on commit
        void addDeferredForeignKeyViolation(ISession& session)
        {
            Wt::Dbo::Session& dboSession{ *getDboSession(session) };
            utils::executeCommand(dboSession, "CREATE TABLE deferred_violation (genre_id INTEGER REFERENCES genre(id) DEFERRABLE INITIALLY DEFERRED)");
            utils::executeCommand(dboSession, "INSERT INTO deferred_violation VALUES (-1)");
        }
    } // namespace

    TEST_F(DatabaseFixture, Transaction_failedWriteCommitThrows)
    {
        EXPECT_THROW(
            {
                auto transaction{ session.createWriteTransaction() };

                Genre::create(session, "Rock");
                addDeferredForeignKeyViolation(session);
            },
            Wt::Dbo::Exception);

        auto transaction{ session.createReadTransaction() };
        EXPECT_EQ(Genre::getCount(session), 0);
    }

    TEST_F(DatabaseFixture, Transaction_failedReadCommitThrows)
    {
        EXPECT_THROW(
            {
                auto transaction{ session.createReadTransaction() };

                addDeferredForeignKeyViolation(session);
            },
            Wt::Dbo::Exception);
    }

    TEST_F(DatabaseFixture, Transaction_exceptionThroughNestedReadRollsBack)
    {
        EXPECT_THROW(
            {
                auto writeTransaction{ session.createWriteTransaction() };

                Genre::create(session, "Rock");

                auto readTransaction{ session.createReadTransaction() };
                throw std::runtime_error{ "error" };
            },
            std::runtime_error);

        auto transaction{ session.createReadTransaction() };
        EXPECT_EQ(Genre::getCount(session), 0);
    }

    TEST_F(DatabaseFixture, Transaction_exceptionThroughWriteRollsBack)
    {
        EXPECT_THROW(
            {
                auto transaction{ session.createWriteTransaction() };

                Genre::create(session, "Rock");
                throw std::runtime_error{ "error" };
            },
            std::runtime_error);

        auto transaction{ session.createReadTransaction() };
        EXPECT_EQ(Genre::getCount(session), 0);
    }

    TEST_F(DatabaseFixture, Transaction_exceptionThroughWriteDoesNotAttemptCommit)
    {
        EXPECT_THROW(
            {
                auto transaction{ session.createWriteTransaction() };

                addDeferredForeignKeyViolation(session);
                throw std::runtime_error{ "error" };
            },
            std::runtime_error);
    }

    TEST_F(DatabaseFixture, Transaction_nestedWritesCommitTogether)
    {
        {
            auto transaction{ session.createWriteTransaction() };

            Genre::create(session, "Rock");
            {
                auto nestedTransaction{ session.createWriteTransaction() };
                Genre::create(session, "Jazz");
            }
            Genre::create(session, "Folk");
        }

        auto transaction{ session.createWriteTransaction() };
        EXPECT_EQ(Genre::getCount(session), 3);

        for (const std::string_view name : { "Rock", "Jazz", "Folk" })
            Genre::destroy(session, std::array{ Genre::find(session, name)->getId() });
    }

    TEST_F(DatabaseFixture, Transaction_writeAfterFailedCommit)
    {
        EXPECT_THROW(
            {
                auto transaction{ session.createWriteTransaction() };

                addDeferredForeignKeyViolation(session);
            },
            Wt::Dbo::Exception);

        ScopedEntity<db::Genre> genre{ session, "Rock" };

        auto transaction{ session.createReadTransaction() };
        EXPECT_EQ(Genre::getCount(session), 1);
    }

    TEST_F(DatabaseFixture, Transaction_exceptionDiscardsUnflushedModification)
    {
        ScopedEntity<db::Genre> genre{ session, "Rock" };

        EXPECT_THROW(
            {
                auto transaction{ session.createWriteTransaction() };

                std::optional<Genre> updatedGenre{ genre.get() };
                updatedGenre->setTrackCount(42);
                Genre::update(session, *updatedGenre);
                throw std::runtime_error{ "error" };
            },
            std::runtime_error);

        auto transaction{ session.createReadTransaction() };
        EXPECT_EQ(genre->getTrackCount(), 0);
    }

    TEST_F(DatabaseFixture, Transaction_exceptionDiscardsFlushedModification)
    {
        ScopedEntity<db::Genre> genre{ session, "Rock" };

        EXPECT_THROW(
            {
                auto transaction{ session.createWriteTransaction() };

                std::optional<Genre> updatedGenre{ genre.get() };
                updatedGenre->setTrackCount(42);
                Genre::update(session, *updatedGenre);
                getDboSession(session)->flush();
                throw std::runtime_error{ "error" };
            },
            std::runtime_error);

        auto transaction{ session.createReadTransaction() };
        EXPECT_EQ(genre->getTrackCount(), 0);
    }

    TEST_F(DatabaseFixture, Transaction_exceptionKeepsRemovedObject)
    {
        ScopedEntity<db::Genre> genre{ session, "Rock" };

        EXPECT_THROW(
            {
                auto transaction{ session.createWriteTransaction() };

                Genre::destroy(session, std::array{ genre.getId() });
                throw std::runtime_error{ "error" };
            },
            std::runtime_error);

        auto transaction{ session.createReadTransaction() };
        EXPECT_TRUE(Genre::find(session, genre.getId()));
    }

    TEST_F(DatabaseFixture, Transaction_failedCommitDiscardsModification)
    {
        ScopedEntity<db::Genre> genre{ session, "Rock" };

        EXPECT_THROW(
            {
                auto transaction{ session.createWriteTransaction() };

                std::optional<Genre> updatedGenre{ genre.get() };
                updatedGenre->setTrackCount(42);
                Genre::update(session, *updatedGenre);
                addDeferredForeignKeyViolation(session);
            },
            Wt::Dbo::Exception);

        auto transaction{ session.createReadTransaction() };
        EXPECT_EQ(genre->getTrackCount(), 0);
    }

    TEST_F(DatabaseFixture, Transaction_exceptionThroughNestedReadDiscardsModification)
    {
        ScopedEntity<db::Genre> genre{ session, "Rock" };

        EXPECT_THROW(
            {
                auto writeTransaction{ session.createWriteTransaction() };

                std::optional<Genre> updatedGenre{ genre.get() };
                updatedGenre->setTrackCount(42);
                Genre::update(session, *updatedGenre);

                auto readTransaction{ session.createReadTransaction() };
                throw std::runtime_error{ "error" };
            },
            std::runtime_error);

        auto transaction{ session.createReadTransaction() };
        EXPECT_EQ(genre->getTrackCount(), 0);
    }
} // namespace lms::db::tests
