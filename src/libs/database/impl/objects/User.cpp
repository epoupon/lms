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

#include "database/objects/User.hpp"

#include <cassert>

#include <Wt/Dbo/Impl.h>
#include <Wt/Dbo/WtSqlTraits.h>

#include "AuthTokenDbo.hpp"
#include "DboAccess.hpp"
#include "Session.hpp"
#include "UIStateDbo.hpp"
#include "UserDbo.hpp"
#include "Utils.hpp"
#include "traits/EnumSetTraits.hpp"
#include "traits/IdTypeTraits.hpp"
#include "traits/StringViewTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::User)

namespace lms::db
{
    User::User(std::string_view loginName)
        : _loginName{ loginName }
    {
    }

    void User::destroy(ISession& session, std::span<const UserId> ids)
    {
        utils::destroyObjects<dbo::User>(session, ids);
    }

    User User::create(ISession& session, std::string_view loginName)
    {
        return utils::addObject(session, std::unique_ptr<dbo::User>{ new dbo::User{ loginName } });
    }

    std::size_t User::getCount(ISession& session)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM user"));
    }

    std::vector<UserId> User::find(ISession& session, const FindParameters& params)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<UserId>("SELECT id FROM user") };

        return utils::execRangeQuery<UserId>(query, params.range);
    }

    void User::find(ISession& session, const FindParameters& params, const std::function<void(const User&)>& func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->find<dbo::User>() };

        return utils::forEachQueryRangeObject(query, params.range, func);
    }

    std::optional<User> User::findDemoUser(ISession& session)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleObject(getDboSession(session)->query<Wt::Dbo::ptr<dbo::User>>("SELECT u from user u").where("u.type = ?").bind(UserType::DEMO));
    }

    std::optional<User> User::find(ISession& session, UserId id)
    {
        checkReadTransaction(session);

        if (!id.isValid())
            return std::nullopt;

        return utils::fetchQuerySingleObject(getDboSession(session)->query<Wt::Dbo::ptr<dbo::User>>("SELECT u from user u").where("u.id = ?").bind(id));
    }

    std::optional<User> User::find(ISession& session, std::string_view name)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleObject(getDboSession(session)->query<Wt::Dbo::ptr<dbo::User>>("SELECT u from user u").where("u.login_name = ?").bind(name));
    }

    void User::setSubsonicDefaultTranscodingOutputBitrate(Bitrate bitrate)
    {
        assert(isAudioBitrateAllowed(bitrate));
        _subsonicDefaultTranscodingOutputBitrate = bitrate;
    }

    void User::setScrobblingBackends(core::EnumSet<ScrobblingBackend> backends)
    {
        _scrobblingBackends = backends;
    }

    void User::setFeedbackBackends(core::EnumSet<FeedbackBackend> backends)
    {
        _feedbackBackends = backends;
    }

    std::size_t User::getAuthTokensCount(ISession& session, UserId id)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM auth_token").where("user_id = ?").bind(id));
    }

    void User::update(ISession& session, const User& user)
    {
        utils::updateObject<dbo::User>(session, user);
    }
} // namespace lms::db
