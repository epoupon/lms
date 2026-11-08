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

#include "database/objects/AuthToken.hpp"

#include <Wt/Dbo/Impl.h>
#include <Wt/Dbo/WtSqlTraits.h>

#include "AuthTokenDbo.hpp"
#include "ObjectPtrImpl.hpp"
#include "Session.hpp"
#include "UserDbo.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/StringViewTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::AuthToken)

namespace lms::db
{
    AuthToken::AuthToken(std::string_view domain, std::string_view value, const Wt::WDateTime& expiry, std::optional<long> maxUseCount)
        : _domain{ domain }
        , _value{ value }
        , _expiry{ expiry }
        , _maxUseCount{ maxUseCount }
    {
    }

    dbo::AuthToken::AuthToken(std::string_view domain, std::string_view value, const Wt::WDateTime& expiry, std::optional<long> maxUseCount, const ObjectPtr<db::User>& user)
        : db::AuthToken{ domain, value, expiry, maxUseCount }
        , _user{ getDboPtr(user) }
    {
    }

    AuthToken::pointer AuthToken::create(ISession& session, std::string_view domain, std::string_view value, const Wt::WDateTime& expiry, std::optional<long> maxUseCount, const ObjectPtr<User>& user)
    {
        return utils::addObject(session, std::unique_ptr<dbo::AuthToken>{ new dbo::AuthToken{ domain, value, expiry, maxUseCount, user } });
    }

    std::size_t AuthToken::getCount(ISession& session)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM auth_token"));
    }

    AuthToken::pointer AuthToken::find(ISession& session, AuthTokenId id)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<Wt::Dbo::ptr<dbo::AuthToken>>("SELECT a_t from auth_token a_t").where("a_t.id = ?").bind(id));
    }

    AuthToken::pointer AuthToken::find(ISession& session, std::string_view domain, std::string_view value)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->find<dbo::AuthToken>() };
        query.where("domain = ?").bind(domain);
        query.where("value = ?").bind(value);

        return utils::fetchQuerySingleResult(query);
    }

    void AuthToken::find(ISession& session, std::string_view domain, UserId userId, std::function<void(const AuthToken::pointer&)> visitor)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->find<dbo::AuthToken>() };
        query.where("domain = ?").bind(domain);
        query.where("user_id = ?").bind(userId);

        utils::forEachQueryResult(query, visitor);
    }

    void AuthToken::removeExpiredTokens(ISession& session, std::string_view domain, const Wt::WDateTime& now)
    {
        checkWriteTransaction(session);

        utils::executeCommand(*getDboSession(session), "DELETE FROM auth_token WHERE expiry < ? AND domain = ?", now, domain);
    }

    void AuthToken::clearUserTokens(ISession& session, std::string_view domain, UserId user)
    {
        checkWriteTransaction(session);

        utils::executeCommand(*getDboSession(session), "DELETE FROM auth_token WHERE user_id = ? AND domain = ?", user, domain);
    }

    ObjectPtr<User> AuthToken::getUser() const
    {
        return getDbo(*this)._user;
    }
} // namespace lms::db
