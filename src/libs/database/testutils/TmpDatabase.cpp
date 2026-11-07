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

#include <Wt/Dbo/Impl.h>

#include "database/IDb.hpp"

#include "Schema.hpp"
#include "Session.hpp"
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
        const std::unique_ptr<ISession> session{ _db->createSession() };
        Schema::createTablesIfNeeded(*session);
        Schema::createIndexesIfNeeded(*session);
    }

    void executeStatement(ISession& session, std::string_view statement)
    {
        checkWriteTransaction(session);

        utils::executeCommand(*getDboSession(session), std::string{ statement });
    }

    bool areAllTablesEmpty(ISession& session)
    {
        checkReadTransaction(session);

        Wt::Dbo::Session& dboSession{ *getDboSession(session) };
        const std::vector<std::string> entryList{ utils::fetchQueryResults(dboSession.query<std::string>("SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'")) };

        return std::all_of(entryList.cbegin(), entryList.cend(), [&](const std::string& entry) {
            const auto count{ utils::fetchQuerySingleResult(dboSession.query<long>("SELECT COUNT(*) FROM " + entry)) };
            return count == 0;
        });
    }
} // namespace lms::db::tests
