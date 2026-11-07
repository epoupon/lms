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

#include "TmpDatabase.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include "database/IDb.hpp"
#include "database/Session.hpp"

#include "Schema.hpp"
#include "Utils.hpp"

namespace lms::db::tests
{
    TmpDatabase::TmpDatabase()
        : _directory{ "lms-database-test-" }
        , _db{ createDb(_directory / "lms.db") }
    {
    }

    TmpDatabase::~TmpDatabase() = default;

    void TmpDatabase::createTablesAndIndexes()
    {
        Session session{ *_db };
        Schema::createTablesIfNeeded(session);
        Schema::createIndexesIfNeeded(session);
    }

    void executeStatement(Session& session, std::string_view statement)
    {
        session.checkWriteTransaction();

        utils::executeCommand(*session.getDboSession(), std::string{ statement });
    }

    bool areAllTablesEmpty(Session& session)
    {
        session.checkReadTransaction();

        Wt::Dbo::Session& dboSession{ *session.getDboSession() };
        const std::vector<std::string> entryList{ utils::fetchQueryResults(dboSession.query<std::string>("SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'")) };

        return std::all_of(entryList.cbegin(), entryList.cend(), [&](const std::string& entry) {
            const auto count{ utils::fetchQuerySingleResult(dboSession.query<long>("SELECT COUNT(*) FROM " + entry)) };
            return count == 0;
        });
    }
} // namespace lms::db::tests
