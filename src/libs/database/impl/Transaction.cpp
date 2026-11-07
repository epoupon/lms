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

#include "database/Transaction.hpp"

#include "core/RecursiveSharedMutex.hpp"

#include "Session.hpp"
#include "TransactionChecker.hpp"

namespace lms::db
{
    WriteTransaction::WriteTransaction(core::RecursiveSharedMutex& mutex, Session& session)
        : _session{ session }
        , _lock{ mutex }
        , _trace{ "Database", core::tracing::Level::Detailed, "WriteTransaction" }
    {
        _session.openTransaction();

#if LMS_CHECK_TRANSACTION_ACCESSES
        TransactionChecker::pushWriteTransaction(*_session.getDboSession());
#endif
    }

    WriteTransaction::~WriteTransaction() noexcept(false)
    {
#if LMS_CHECK_TRANSACTION_ACCESSES
        TransactionChecker::popWriteTransaction(*_session.getDboSession());
#endif

        _session.closeTransaction();
    }

    ReadTransaction::ReadTransaction(Session& session)
        : _session{ session }
        , _trace{ "Database", core::tracing::Level::Detailed, "ReadTransaction" }
    {
        _session.openTransaction();

#if LMS_CHECK_TRANSACTION_ACCESSES
        TransactionChecker::pushReadTransaction(*_session.getDboSession());
#endif
    }

    ReadTransaction::~ReadTransaction() noexcept(false)
    {
#if LMS_CHECK_TRANSACTION_ACCESSES
        TransactionChecker::popReadTransaction(*_session.getDboSession());
#endif

        _session.closeTransaction();
    }
} // namespace lms::db
