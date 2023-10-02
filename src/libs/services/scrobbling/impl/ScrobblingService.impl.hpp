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

#pragma once

#include "services/database/Db.hpp"
#include "services/database/Session.hpp"
#include "services/database/User.hpp"

namespace Scrobbling
{
    using namespace Database;

    template <typename ObjType, typename ObjIdType, typename StarredObjType>
    void ScrobblingService::star(UserId userId, ObjIdType objId)
    {
        auto scrobbler {getUserScrobbler(userId)};
        if (!scrobbler)
            return;

        typename StarredObjType::IdType starredObjId;
        {
            Session& session {_db.getTLSSession()};
            auto transaction {session.createUniqueTransaction()};

            typename StarredObjType::pointer starredObj {StarredObjType::find(session, objId, userId, *scrobbler)};
            if (!starredObj)
            {
                const typename ObjType::pointer obj {ObjType::find(session, objId)};
                if (!obj)
                    return;

                const User::pointer user {User::find(session, userId)};
                if (!user)
                    return;

                starredObj = session.create<StarredObjType>(obj, user, *scrobbler);
            }
            starredObj.modify()->setDateTime(Wt::WDateTime::currentDateTime());
            starredObjId = starredObj->getId();
        }
        _scrobblers[*scrobbler]->onStarred(starredObjId);
    }

    template <typename ObjType, typename ObjIdType, typename StarredObjType>
    void ScrobblingService::unstar(UserId userId, ObjIdType objId)
    {
        auto scrobbler {getUserScrobbler(userId)};
        if (!scrobbler)
            return;

        typename StarredObjType::IdType starredObjId;
        {
            Session& session {_db.getTLSSession()};
            auto transaction {session.createSharedTransaction()};

            typename StarredObjType::pointer starredObj {StarredObjType::find(session, objId, userId, *scrobbler)};
            if (!starredObj)
                return;

            starredObjId = starredObj->getId();
        }
        _scrobblers[*scrobbler]->onUnstarred(starredObjId);
    }

    template <typename ObjType, typename ObjIdType, typename StarredObjType>
    bool ScrobblingService::isStarred(UserId userId, ObjIdType objId)
    {
        auto scrobbler {getUserScrobbler(userId)};
        if (!scrobbler)
            return false;

        Session& session {_db.getTLSSession()};
        auto transaction {session.createSharedTransaction()};

        typename StarredObjType::pointer starredObj {StarredObjType::find(session, objId, userId, *scrobbler)};
        return starredObj && (starredObj->getScrobblingState() != ScrobblingState::PendingRemove);
    }

    template <typename ObjType, typename ObjIdType, typename StarredObjType>
    Wt::WDateTime ScrobblingService::getStarredDateTime(UserId userId, ObjIdType objId)
    {
        auto scrobbler {getUserScrobbler(userId)};
        if (!scrobbler)
            return {};

        Session& session {_db.getTLSSession()};
        auto transaction {session.createSharedTransaction()};

        typename StarredObjType::pointer starredObj {StarredObjType::find(session, objId, userId, *scrobbler)};
        if (starredObj && (starredObj->getScrobblingState() != ScrobblingState::PendingRemove))
            return starredObj->getDateTime();

        return {};
    }

} // ns Scrobbling

