/*
 * Copyright (C) 2013 Emeric Poupon
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

#include <deque>

#include <Wt/Dbo/Session.h>
#include <Wt/Dbo/Transaction.h>

#include "database/ISession.hpp"

#include "TransactionChecker.hpp"

namespace lms::db
{
    class Db;

    class Session final : public ISession
    {
    public:
        explicit Session(Db& db);
        ~Session() override = default;
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;

        WriteTransaction createWriteTransaction() override;
        ReadTransaction createReadTransaction() override;

        // returning a ptr here to ease further wrapping using operator->
        Wt::Dbo::Session* getDboSession() { return &_session; }

        Db& getDb() { return _db; }

    private:
        friend class WriteTransaction;
        friend class ReadTransaction;

        void openTransaction();
        void closeTransaction();

        Db& _db;
        Wt::Dbo::Session _session;
        std::deque<Wt::Dbo::Transaction> _transactions;
    };

    inline Wt::Dbo::Session* getDboSession(ISession& session)
    {
        return static_cast<Session&>(session).getDboSession();
    }

    inline void checkWriteTransaction([[maybe_unused]] ISession& session)
    {
#if LMS_CHECK_TRANSACTION_ACCESSES
        TransactionChecker::checkWriteTransaction(*getDboSession(session));
#endif
    }

    inline void checkReadTransaction([[maybe_unused]] ISession& session)
    {
#if LMS_CHECK_TRANSACTION_ACCESSES
        TransactionChecker::checkReadTransaction(*getDboSession(session));
#endif
    }
} // namespace lms::db
