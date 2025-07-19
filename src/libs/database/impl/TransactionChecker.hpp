/*
 * Copyright (C) 2023 Emeric Poupon
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

#if !defined(NDEBUG)
    #define LMS_CHECK_TRANSACTION_ACCESSES 1
#else
    #define LMS_CHECK_TRANSACTION_ACCESSES 0
#endif

#if LMS_CHECK_TRANSACTION_ACCESSES

namespace Wt::Dbo
{
    class Session;
}

namespace lms::db
{
    class Session;

    class TransactionChecker
    {
    public:
        enum class TransactionType
        {
            Read,
            Write,
        };

        static void pushWriteTransaction(const Wt::Dbo::Session& session);
        static void pushReadTransaction(const Wt::Dbo::Session& session);

        static void popWriteTransaction(const Wt::Dbo::Session& session);
        static void popReadTransaction(const Wt::Dbo::Session& session);

        static void checkWriteTransaction(const Wt::Dbo::Session& session);
        static void checkWriteTransaction(const Session& session);
        static void checkReadTransaction(const Wt::Dbo::Session& session);
        static void checkReadTransaction(const Session& session);

    private:
        static void pushTransaction(TransactionType type, const Wt::Dbo::Session& session);
        static void popTransaction(TransactionType type, const Wt::Dbo::Session& session);
    };
} // namespace lms::db

#endif