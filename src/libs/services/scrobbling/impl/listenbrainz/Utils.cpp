/*
 * Copyright (C) 2021 Emeric Poupon
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

#include "Utils.hpp"

#include <Wt/Json/Object.h>
#include <Wt/Json/Parser.h>

#include "database/Session.hpp"
#include "database/objects/User.hpp"

namespace lms::scrobbling::listenBrainz::utils
{
    std::string getListenBrainzToken(db::Session& session, db::UserId userId)
    {
        std::string res;

        {
            auto transaction{ session.createReadTransaction() };

            if (const db::User::pointer user{ db::User::find(session, userId) })
                res = user->getListenBrainzToken();
        }

        return res;
    }

    std::string parseValidateToken(std::string_view msgBody)
    {
        std::string listenBrainzUserName;

        Wt::Json::ParseError error;
        Wt::Json::Object root;
        if (!Wt::Json::parse(std::string{ msgBody }, root, error))
        {
            LMS_LOG_LISTENBRAINZ(ERROR, "Cannot parse 'validate-token' result: " << error.what());
            return listenBrainzUserName;
        }

        if (!root.get("valid").orIfNull(false))
        {
            LMS_LOG_LISTENBRAINZ(INFO, "Invalid listenbrainz user");
            return listenBrainzUserName;
        }

        listenBrainzUserName = root.get("user_name").orIfNull("");
        return listenBrainzUserName;
    }
} // namespace lms::scrobbling::listenBrainz::utils
