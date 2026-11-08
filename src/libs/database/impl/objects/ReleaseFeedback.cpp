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

#include "database/objects/ReleaseFeedback.hpp"

#include <Wt/Dbo/Impl.h>
#include <Wt/Dbo/WtSqlTraits.h>

#include "ObjectPtrImpl.hpp"
#include "ReleaseDbo.hpp"
#include "ReleaseFeedbackDbo.hpp"
#include "Session.hpp"
#include "UserDbo.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::ReleaseFeedback)

namespace lms::db
{
    dbo::ReleaseFeedback::ReleaseFeedback(const ObjectPtr<db::Release>& release, const ObjectPtr<db::User>& user)
        : _release{ getDboPtr(release) }
        , _user{ getDboPtr(user) }
    {
    }

    ReleaseFeedback::pointer ReleaseFeedback::create(ISession& session, const ObjectPtr<Release>& release, const ObjectPtr<User>& user)
    {
        return utils::addObject(session, std::unique_ptr<dbo::ReleaseFeedback>{ new dbo::ReleaseFeedback{ release, user } });
    }

    std::size_t ReleaseFeedback::getCount(ISession& session)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM release_feedback"));
    }

    ReleaseFeedback::pointer ReleaseFeedback::find(ISession& session, ReleaseFeedbackId id)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->find<dbo::ReleaseFeedback>().where("id = ?").bind(id));
    }

    ReleaseFeedback::pointer ReleaseFeedback::find(ISession& session, ReleaseId releaseId, UserId userId)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->find<dbo::ReleaseFeedback>().where("release_id = ?").bind(releaseId).where("user_id = ?").bind(userId));
    }

    void ReleaseFeedback::setDateTime(const Wt::WDateTime& dateTime)
    {
        _dateTime = utils::normalizeDateTime(dateTime);
    }

    ObjectPtr<Release> ReleaseFeedback::getRelease() const
    {
        return getDbo(*this)._release;
    }

    ObjectPtr<User> ReleaseFeedback::getUser() const
    {
        return getDbo(*this)._user;
    }
} // namespace lms::db
