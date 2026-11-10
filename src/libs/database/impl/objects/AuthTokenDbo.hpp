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

#pragma once

#include <Wt/Dbo/Dbo.h>
#include <Wt/Dbo/Field.h>

#include "database/objects/AuthToken.hpp"

#include "DboAccess.hpp"

namespace lms::db::dbo
{
    class User;

    class AuthToken final : public db::AuthToken, public Wt::Dbo::Dbo<AuthToken>
    {
    public:
        AuthToken() = default;
        using db::AuthToken::AuthToken;

        AuthToken(std::string_view domain, std::string_view value, const Wt::WDateTime& expiry, std::optional<long> maxUseCount, Wt::Dbo::ptr<User> user);

        void syncIds()
        {
            _id = id();
            _userId = _user.id();
        }

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _domain, "domain");
            Wt::Dbo::field(a, _value, "value");
            Wt::Dbo::field(a, _expiry, "expiry");
            Wt::Dbo::field(a, _useCount, "use_count");
            Wt::Dbo::field(a, _lastUsed, "last_used");
            Wt::Dbo::field(a, _maxUseCount, "max_use_count");
            Wt::Dbo::belongsTo(a, _user, "user", Wt::Dbo::OnDeleteCascade);
        }

        Wt::Dbo::ptr<User> _user;
    };
} // namespace lms::db::dbo

namespace lms::db
{
    template<>
    struct DboOf<AuthToken>
    {
        using type = dbo::AuthToken;
    };
} // namespace lms::db
