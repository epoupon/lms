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

#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/User.hpp"

namespace lms::feedback
{
    using namespace db;

    template<typename ObjType, typename ObjIdType, typename StarredObjType>
    void FeedbackService::star(UserId userId, ObjIdType objId)
    {
        const auto backend{ getUserFeedbackBackend(userId) };
        if (!backend)
            return;

        typename StarredObjType::IdType starredObjId;
        {
            Session& session{ _db.getTLSSession() };
            auto transaction{ session.createWriteTransaction() };

            typename StarredObjType::pointer starredObj{ StarredObjType::find(session, objId, userId, *backend) };
            if (!starredObj)
            {
                const typename ObjType::pointer obj{ ObjType::find(session, objId) };
                if (!obj)
                    return;

                const User::pointer user{ User::find(session, userId) };
                if (!user)
                    return;

                starredObj = session.create<StarredObjType>(obj, user, *backend);
            }
            starredObj.modify()->setDateTime(Wt::WDateTime::currentDateTime());
            starredObjId = starredObj->getId();
        }
        _backends[*backend]->onStarred(starredObjId);
    }

    template<typename ObjType, typename ObjIdType, typename StarredObjType>
    void FeedbackService::unstar(UserId userId, ObjIdType objId)
    {
        const auto backend{ getUserFeedbackBackend(userId) };
        if (!backend)
            return;

        typename StarredObjType::IdType starredObjId;
        {
            Session& session{ _db.getTLSSession() };
            auto transaction{ session.createReadTransaction() };

            typename StarredObjType::pointer starredObj{ StarredObjType::find(session, objId, userId, *backend) };
            if (!starredObj)
                return;

            starredObjId = starredObj->getId();
        }
        _backends[*backend]->onUnstarred(starredObjId);
    }

    template<typename ObjType, typename ObjIdType, typename StarredObjType>
    bool FeedbackService::isStarred(UserId userId, ObjIdType objId)
    {
        Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        typename StarredObjType::pointer starredObj{ StarredObjType::find(session, objId, userId) };
        return starredObj && (starredObj->getSyncState() != SyncState::PendingRemove);
    }

    template<typename ObjType, typename ObjIdType, typename StarredObjType>
    Wt::WDateTime FeedbackService::getStarredDateTime(UserId userId, ObjIdType objId)
    {
        Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        typename StarredObjType::pointer starredObj{ StarredObjType::find(session, objId, userId) };
        if (starredObj && (starredObj->getSyncState() != SyncState::PendingRemove))
            return starredObj->getDateTime();

        return {};
    }

    template<typename ObjType, typename ObjIdType, typename RatedObjType>
    void FeedbackService::setRating(db::UserId userId, ObjIdType objectId, std::optional<db::Rating> rating)
    {
        Session& session{ _db.getTLSSession() };
        auto transaction{ session.createWriteTransaction() };

        typename RatedObjType::pointer ratedObject{ RatedObjType::find(session, objectId, userId) };
        if (rating)
        {
            if (!ratedObject)
            {
                typename ObjType::pointer obj{ ObjType::find(session, objectId) };
                const User::pointer user{ User::find(session, userId) };

                if (!obj || !user)
                    return;

                ratedObject = session.create<RatedObjType>(obj, user);
            }

            ratedObject.modify()->setRating(*rating);
            ratedObject.modify()->setLastUpdated(Wt::WDateTime::currentDateTime());
        }
        else
        {
            if (ratedObject)
                ratedObject.remove();
        }
    }

    template<typename ObjType, typename ObjIdType, typename RatedObjType>
    std::optional<db::Rating> FeedbackService::getRating(db::UserId userId, ObjIdType objectId)
    {
        Session& session{ _db.getTLSSession() };
        auto transaction{ session.createReadTransaction() };

        const typename RatedObjType::pointer ratedObj{ RatedObjType::find(session, objectId, userId) };
        if (!ratedObj)
            return std::nullopt;

        return ratedObj->getRating();
    }

} // namespace lms::feedback