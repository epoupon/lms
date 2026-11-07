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

#include <string_view>

#include <Wt/Dbo/Exception.h>
#include <Wt/Dbo/Impl.h>

#include "core/Service.hpp"

#include "database/profiling/IQueryProfiler.hpp"

#include "Common.hpp"
#include "Utils.hpp"

namespace lms::db::tests
{
    TEST_F(DatabaseFixture, QueryProfiler_recordsQuery)
    {
        const core::Service<IQueryProfiler> profiler{ createQueryProfiler() };

        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_EQ(Genre::getCount(session), 0);
            EXPECT_EQ(Genre::getCount(session), 0);
        }

        std::size_t recordedQueryCount{};
        profiler->visitQueries([&](const IQueryProfiler::QueryStats& stats) {
            recordedQueryCount++;
            EXPECT_EQ(stats.callCount, std::size_t{ 2 });
            EXPECT_FALSE(stats.plan.empty());
        });
        EXPECT_EQ(recordedQueryCount, std::size_t{ 1 });
    }

    TEST_F(DatabaseFixture, QueryProfiler_failedQueryIsNotRecorded)
    {
        const core::Service<IQueryProfiler> profiler{ createQueryProfiler() };

        GenreId genreId;
        {
            auto transaction{ session.createWriteTransaction() };

            EXPECT_THROW(utils::fetchQueryResults(getDboSession(session)->query<int>("SELECT 1 FROM not_a_table")), Wt::Dbo::Exception);
            genreId = Genre::create(session, "Rock")->getId();
        }

        // The caller caught the error: its transaction must not have been rolled back
        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_TRUE(Genre::find(session, genreId));
        }

        {
            auto transaction{ session.createWriteTransaction() };

            const std::vector<GenreId> ids{ genreId };
            Genre::destroy(session, ids);
        }

        profiler->visitQueries([&](const IQueryProfiler::QueryStats& stats) {
            EXPECT_EQ(stats.query.find("not_a_table"), std::string_view::npos);
        });
    }
} // namespace lms::db::tests
