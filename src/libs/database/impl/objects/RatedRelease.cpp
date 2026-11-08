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

#include "database/objects/RatedRelease.hpp"

#include <Wt/Dbo/Impl.h>
#include <Wt/Dbo/WtSqlTraits.h>

#include "ObjectPtrImpl.hpp"
#include "RatedReleaseDbo.hpp"
#include "ReleaseDbo.hpp"
#include "Session.hpp"
#include "UserDbo.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::RatedRelease)

namespace lms::db
{
    dbo::RatedRelease::RatedRelease(const ObjectPtr<db::Release>& release, const ObjectPtr<db::User>& user)
        : _release{ getDboPtr(release) }
        , _user{ getDboPtr(user) }
    {
    }

    RatedRelease::pointer RatedRelease::create(ISession& session, const ObjectPtr<Release>& release, const ObjectPtr<User>& user)
    {
        return utils::addObject(session, std::unique_ptr<dbo::RatedRelease>{ new dbo::RatedRelease{ release, user } });
    }

    std::size_t RatedRelease::getCount(ISession& session)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM rated_release"));
    }

    RatedRelease::pointer RatedRelease::find(ISession& session, RatedReleaseId id)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleObject(getDboSession(session)->find<dbo::RatedRelease>().where("id = ?").bind(id));
    }

    RatedRelease::pointer RatedRelease::find(ISession& session, ReleaseId releaseId, UserId userId)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleObject(getDboSession(session)->find<dbo::RatedRelease>().where("release_id = ?").bind(releaseId).where("user_id = ?").bind(userId));
    }

    void RatedRelease::find(ISession& session, const FindParameters& params, std::function<void(const pointer&)> func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<Wt::Dbo::ptr<dbo::RatedRelease>>("SELECT r_r FROM rated_release r_r") };

        if (params.user.isValid())
            query.where("r_r.user_id = ?").bind(params.user);

        utils::forEachQueryRangeObject(query, params.range, func);
    }

    void RatedRelease::setLastUpdated(const Wt::WDateTime& lastUpdated)
    {
        _lastUpdated = utils::normalizeDateTime(lastUpdated);
    }

    ObjectPtr<Release> RatedRelease::getRelease() const
    {
        return toObjectPtr(getDbo(*this)._release);
    }

    ObjectPtr<User> RatedRelease::getUser() const
    {
        return toObjectPtr(getDbo(*this)._user);
    }
} // namespace lms::db
