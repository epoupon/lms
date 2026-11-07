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

#include <Wt/Dbo/Session.h>

#include <span>
#include <string>
#include <string_view>

#include "database/Transaction.hpp"

namespace lms::db
{
    class Db;
    class IDb;
    class Session
    {
    public:
        explicit Session(IDb& db);
        ~Session() = default;
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;

        [[nodiscard]] WriteTransaction createWriteTransaction();
        [[nodiscard]] ReadTransaction createReadTransaction();

        void checkWriteTransaction() const;
        void checkReadTransaction() const;

        // returning a ptr here to ease further wrapping using operator->
        Wt::Dbo::Session* getDboSession() { return &_session; }
        const Wt::Dbo::Session* getDboSession() const { return &_session; }

        Db& getDb() { return _db; }

        template<typename Object, typename... Args>
        typename Object::pointer create(Args&&... args)
        {
            checkWriteTransaction();

            typename Object::pointer res{ Object::create(*this, std::forward<Args>(args)...) };
            getDboSession()->flush();

            return res;
        }

        template<typename Object>
        void destroy(typename Object::IdType id)
        {
            destroy<Object>(std::span{ &id, 1 });
        }

        template<typename Object>
        void destroy(std::span<const typename Object::IdType> ids)
        {
            checkWriteTransaction();

            const std::string query{ std::string{ "DELETE FROM " } + _session.tableName<Object>() + " WHERE id = ?" };
            for (typename Object::IdType id : ids)
                executeWithId(query, id.getValue());
        }

    private:
        void executeWithId(std::string_view query, long long id);

        Db& _db;
        Wt::Dbo::Session _session;
    };
} // namespace lms::db
