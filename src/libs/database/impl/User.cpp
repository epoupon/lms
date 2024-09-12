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

#include "database/User.hpp"

#include "core/ILogger.hpp"
#include "database/Artist.hpp"
#include "database/Release.hpp"
#include "database/Session.hpp"
#include "database/Track.hpp"
#include "database/UIState.hpp"

#include "IdTypeTraits.hpp"
#include "StringViewTraits.hpp"
#include "Utils.hpp"

namespace lms::db
{
    User::User(std::string_view loginName)
        : _loginName{ loginName }
    {
    }

    User::pointer User::create(Session& session, std::string_view loginName)
    {
        return session.getDboSession()->add(std::unique_ptr<User>{ new User{ loginName } });
    }

    std::size_t User::getCount(Session& session)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(*) FROM user"));
    }

    RangeResults<UserId> User::find(Session& session, const FindParameters& params)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<UserId>("SELECT id FROM user") };

        if (params.scrobblingBackend)
            query.where("scrobbling_backend = ?").bind(*params.scrobblingBackend);
        if (params.feedbackBackend)
            query.where("feedback_backend = ?").bind(*params.feedbackBackend);

        return utils::execRangeQuery<UserId>(query, params.range);
    }

    void User::find(Session& session, const FindParameters& params, const std::function<void(const User::pointer&)>& func)
    {
        auto query{ session.getDboSession()->find<User>() };

        if (params.scrobblingBackend)
            query.where("scrobbling_backend = ?").bind(*params.scrobblingBackend);
        if (params.feedbackBackend)
            query.where("feedback_backend = ?").bind(*params.feedbackBackend);

        return utils::forEachQueryRangeResult(query, params.range, func);
    }

    User::pointer User::findDemoUser(Session& session)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<User>>("SELECT u from user u").where("u.type = ?").bind(UserType::DEMO));
    }

    User::pointer User::find(Session& session, UserId id)
    {
        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<User>>("SELECT u from user u").where("u.id = ?").bind(id));
    }

    User::pointer User::find(Session& session, std::string_view name)
    {
        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<User>>("SELECT u from user u").where("u.login_name = ?").bind(name));
    }

    void User::setSubsonicDefaultTranscodingOutputBitrate(Bitrate bitrate)
    {
        assert(isAudioBitrateAllowed(bitrate));
        _subsonicDefaultTranscodingOutputBitrate = bitrate;
    }

    void User::clearAuthTokens()
    {
        _authTokens.clear();
    }

} // namespace lms::db
