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

#include "services/database/User.hpp"

#include "services/database/Artist.hpp"
#include "services/database/Release.hpp"
#include "services/database/Session.hpp"
#include "services/database/Track.hpp"
#include "utils/Logger.hpp"
#include "IdTypeTraits.hpp"
#include "StringViewTraits.hpp"
#include "Utils.hpp"

namespace Database {

    User::User(std::string_view loginName)
        : _loginName{ loginName }
    {
    }

    User::pointer User::create(Session& session, std::string_view loginName)
    {
        return session.getDboSession().add(std::unique_ptr<User> {new User{ loginName }});
    }

    std::size_t User::getCount(Session& session)
    {
        session.checkSharedLocked();

        return session.getDboSession().query<int>("SELECT COUNT(*) FROM user");
    }

    RangeResults<UserId> User::find(Session& session, const FindParameters& params)
    {
        session.checkSharedLocked();

        auto query{ session.getDboSession().query<UserId>("SELECT id FROM user") };

        if (params.scrobbler)
            query.where("scrobbler = ?").bind(*params.scrobbler);

        return Utils::execQuery(query, params.range);
    }

    User::pointer User::findDemoUser(Session& session)
    {
        session.checkSharedLocked();

        return session.getDboSession().find<User>().where("type = ?").bind(UserType::DEMO).resultValue();
    }

    User::pointer User::find(Session& session, UserId id)
    {
        return session.getDboSession().find<User>().where("id = ?").bind(id).resultValue();
    }

    User::pointer User::find(Session& session, std::string_view name)
    {
        return session.getDboSession().find<User>()
            .where("login_name = ?").bind(name)
            .resultValue();
    }

    void User::setSubsonicDefaultTranscodeBitrate(Bitrate bitrate)
    {
        assert(isAudioBitrateAllowed(bitrate));
        _subsonicDefaultTranscodeBitrate = bitrate;
    }

    void User::clearAuthTokens()
    {
        _authTokens.clear();
    }

} // namespace Database
