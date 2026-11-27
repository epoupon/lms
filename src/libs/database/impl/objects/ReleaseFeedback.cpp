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

#include "DboAccess.hpp"
#include "ReleaseDbo.hpp"
#include "ReleaseFeedbackDbo.hpp"
#include "Session.hpp"
#include "UserDbo.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::ReleaseFeedback)

namespace lms::db
{
    dbo::ReleaseFeedback::ReleaseFeedback(Wt::Dbo::ptr<Release> release, Wt::Dbo::ptr<User> user)
        : _release{ std::move(release) }
        , _user{ std::move(user) }
    {
    }

    ReleaseFeedback ReleaseFeedback::create(ISession& session, ReleaseId release, UserId user)
    {
        return utils::addObject(session, std::unique_ptr<dbo::ReleaseFeedback>{ new dbo::ReleaseFeedback{ getDboSession(session)->loadLazy<dbo::Release>(release.getValue()), getDboSession(session)->loadLazy<dbo::User>(user.getValue()) } });
    }

    void ReleaseFeedback::destroy(ISession& session, std::span<const ReleaseFeedbackId> ids)
    {
        utils::destroyObjects<dbo::ReleaseFeedback>(session, ids);
    }

    std::size_t ReleaseFeedback::getCount(ISession& session)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM release_feedback"));
    }

    std::optional<ReleaseFeedback> ReleaseFeedback::find(ISession& session, ReleaseFeedbackId id)
    {
        checkReadTransaction(session);

        if (!id.isValid())
            return std::nullopt;

        return utils::fetchQuerySingleObject(getDboSession(session)->find<dbo::ReleaseFeedback>().where("id = ?").bind(id));
    }

    std::optional<ReleaseFeedback> ReleaseFeedback::find(ISession& session, ReleaseId releaseId, UserId userId)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleObject(getDboSession(session)->find<dbo::ReleaseFeedback>().where("release_id = ?").bind(releaseId).where("user_id = ?").bind(userId));
    }

    void ReleaseFeedback::setDateTime(const Wt::WDateTime& dateTime)
    {
        _dateTime = utils::normalizeDateTime(dateTime);
    }

    void ReleaseFeedback::update(ISession& session, const ReleaseFeedback& releaseFeedback)
    {
        utils::updateObject<dbo::ReleaseFeedback>(session, releaseFeedback);
    }
} // namespace lms::db
