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

#include "database/objects/StarredRelease.hpp"

#include <Wt/Dbo/Impl.h>
#include <Wt/Dbo/WtSqlTraits.h>

#include "database/Session.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/User.hpp"

#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::StarredRelease)

namespace lms::db
{
    StarredRelease::StarredRelease(ObjectPtr<Release> release, ObjectPtr<User> user, FeedbackBackend backend)
        : _backend{ backend }
        , _release{ getDboPtr(release) }
        , _user{ getDboPtr(user) }
    {
    }

    StarredRelease::pointer StarredRelease::create(Session& session, ObjectPtr<Release> release, ObjectPtr<User> user, FeedbackBackend backend)
    {
        return session.getDboSession()->add(std::unique_ptr<StarredRelease>{ new StarredRelease{ release, user, backend } });
    }

    std::size_t StarredRelease::getCount(Session& session)
    {
        session.checkReadTransaction();
        return session.getDboSession()->query<int>("SELECT COUNT(*) FROM starred_release");
    }

    StarredRelease::pointer StarredRelease::find(Session& session, StarredReleaseId id)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->find<StarredRelease>().where("id = ?").bind(id));
    }

    StarredRelease::pointer StarredRelease::find(Session& session, ReleaseId releaseId, UserId userId)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->query<Wt::Dbo::ptr<StarredRelease>>("SELECT s_r from starred_release s_r").join("user u ON u.id = s_r.user_id").where("s_r.release_id = ?").bind(releaseId).where("s_r.user_id = ?").bind(userId).where("s_r.backend = u.feedback_backend"));
    }

    StarredRelease::pointer StarredRelease::find(Session& session, ReleaseId releaseId, UserId userId, FeedbackBackend backend)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->find<StarredRelease>().where("release_id = ?").bind(releaseId).where("user_id = ?").bind(userId).where("backend = ?").bind(backend));
    }

    void StarredRelease::setDateTime(const Wt::WDateTime& dateTime)
    {
        _dateTime = utils::normalizeDateTime(dateTime);
    }
} // namespace lms::db
