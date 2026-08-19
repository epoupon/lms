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

#include "database/Session.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/User.hpp"

#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::ReleaseFeedback)

namespace lms::db
{
    ReleaseFeedback::ReleaseFeedback(ObjectPtr<Release> release, ObjectPtr<User> user)
        : _release{ getDboPtr(release) }
        , _user{ getDboPtr(user) }
    {
    }

    ReleaseFeedback::pointer ReleaseFeedback::create(Session& session, ObjectPtr<Release> release, ObjectPtr<User> user)
    {
        return session.getDboSession()->add(std::unique_ptr<ReleaseFeedback>{ new ReleaseFeedback{ release, user } });
    }

    std::size_t ReleaseFeedback::getCount(Session& session)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(*) FROM release_feedback"));
    }

    ReleaseFeedback::pointer ReleaseFeedback::find(Session& session, ReleaseFeedbackId id)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->find<ReleaseFeedback>().where("id = ?").bind(id));
    }

    ReleaseFeedback::pointer ReleaseFeedback::find(Session& session, ReleaseId releaseId, UserId userId)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->find<ReleaseFeedback>().where("release_id = ?").bind(releaseId).where("user_id = ?").bind(userId));
    }

    void ReleaseFeedback::setDateTime(const Wt::WDateTime& dateTime)
    {
        _dateTime = utils::normalizeDateTime(dateTime);
    }
} // namespace lms::db
