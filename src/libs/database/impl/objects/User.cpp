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

#include "database/objects/Artist.hpp"
#include "database/objects/AuthToken.hpp"
#include "database/objects/Genre.hpp"
#include "database/objects/Grouping.hpp"
#include "database/objects/Language.hpp"
#include "database/objects/Mood.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/UIState.hpp"

#include "ObjectPtrImpl.hpp"
#include "Session.hpp"
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

    User::pointer User::create(ISession& session, std::string_view loginName)
    {
        return utils::addObject(session, std::unique_ptr<User>{ new User{ loginName } });
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

    void User::find(ISession& session, const FindParameters& params, const std::function<void(const User::pointer&)>& func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->find<User>() };

        return utils::forEachQueryRangeResult(query, params.range, func);
    }

    User::pointer User::findDemoUser(ISession& session)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<Wt::Dbo::ptr<User>>("SELECT u from user u").where("u.type = ?").bind(UserType::DEMO));
    }

    User::pointer User::find(ISession& session, UserId id)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<Wt::Dbo::ptr<User>>("SELECT u from user u").where("u.id = ?").bind(id));
    }

    User::pointer User::find(ISession& session, std::string_view name)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->query<Wt::Dbo::ptr<User>>("SELECT u from user u").where("u.login_name = ?").bind(name));
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
} // namespace lms::db
