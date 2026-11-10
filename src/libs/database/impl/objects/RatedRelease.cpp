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

#include "DboAccess.hpp"
#include "RatedReleaseDbo.hpp"
#include "ReleaseDbo.hpp"
#include "Session.hpp"
#include "UserDbo.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::RatedRelease)

namespace lms::db
{
    dbo::RatedRelease::RatedRelease(Wt::Dbo::ptr<Release> release, Wt::Dbo::ptr<User> user)
        : _release{ std::move(release) }
        , _user{ std::move(user) }
    {
    }

    RatedRelease RatedRelease::create(ISession& session, ReleaseId release, UserId user)
    {
        return utils::addObject(session, std::unique_ptr<dbo::RatedRelease>{ new dbo::RatedRelease{ getDboSession(session)->loadLazy<dbo::Release>(release.getValue()), getDboSession(session)->loadLazy<dbo::User>(user.getValue()) } });
    }

    void RatedRelease::destroy(ISession& session, std::span<const RatedReleaseId> ids)
    {
        utils::destroyObjects<dbo::RatedRelease>(session, ids);
    }

    std::size_t RatedRelease::getCount(ISession& session)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM rated_release"));
    }

    std::optional<RatedRelease> RatedRelease::find(ISession& session, RatedReleaseId id)
    {
        checkReadTransaction(session);

        if (!id.isValid())
            return std::nullopt;

        return utils::fetchQuerySingleObject(getDboSession(session)->find<dbo::RatedRelease>().where("id = ?").bind(id));
    }

    std::optional<RatedRelease> RatedRelease::find(ISession& session, ReleaseId releaseId, UserId userId)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleObject(getDboSession(session)->find<dbo::RatedRelease>().where("release_id = ?").bind(releaseId).where("user_id = ?").bind(userId));
    }

    void RatedRelease::find(ISession& session, const FindParameters& params, const std::function<void(const RatedRelease&)>& func)
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

    void RatedRelease::update(ISession& session, const RatedRelease& ratedRelease)
    {
        utils::updateObject<dbo::RatedRelease>(session, ratedRelease);
    }
} // namespace lms::db
