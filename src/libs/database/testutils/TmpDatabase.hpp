/*
 * Copyright (C) 2021 Emeric Poupon
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

#pragma once

#include <memory>

#include "core/ScopedTmpDirectory.hpp"

namespace lms::db
{
    class IDb;
}

// No test framework dependency: also used by benchmarks, which are built without tests
namespace lms::db::tests
{
    class TmpDatabase final
    {
    public:
        TmpDatabase();
        ~TmpDatabase();

        TmpDatabase(const TmpDatabase&) = delete;
        TmpDatabase& operator=(const TmpDatabase&) = delete;

        IDb& getDb() { return *_db; }

    private:
        // Declared first: the database must be closed before its directory is removed
        core::ScopedTmpDirectory _directory;
        std::unique_ptr<IDb> _db;
    };
} // namespace lms::db::tests
