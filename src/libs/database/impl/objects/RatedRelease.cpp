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

#include "database/Session.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/User.hpp"

#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::RatedRelease)

namespace lms::db
{
    RatedRelease::RatedRelease(ObjectPtr<Release> release, ObjectPtr<User> user)
        : _release{ getDboPtr(release) }
        , _user{ getDboPtr(user) }
    {
    }

    RatedRelease::pointer RatedRelease::create(Session& session, ObjectPtr<Release> release, ObjectPtr<User> user)
    {
        return session.getDboSession()->add(std::unique_ptr<RatedRelease>{ new RatedRelease{ release, user } });
    }

    std::size_t RatedRelease::getCount(Session& session)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(*) FROM rated_release"));
    }

    RatedRelease::pointer RatedRelease::find(Session& session, RatedReleaseId id)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->find<RatedRelease>().where("id = ?").bind(id));
    }

    RatedRelease::pointer RatedRelease::find(Session& session, ReleaseId releaseId, UserId userId)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->find<RatedRelease>().where("release_id = ?").bind(releaseId).where("user_id = ?").bind(userId));
    }

    void RatedRelease::find(Session& session, const FindParameters& params, std::function<void(const pointer&)> func)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<Wt::Dbo::ptr<RatedRelease>>("SELECT r_r FROM rated_release r_r") };

        if (params.user.isValid())
            query.where("r_r.user_id = ?").bind(params.user);

        utils::forEachQueryRangeResult(query, params.range, func);
    }

    void RatedRelease::setLastUpdated(const Wt::WDateTime& lastUpdated)
    {
        _lastUpdated = utils::normalizeDateTime(lastUpdated);
    }
} // namespace lms::db
