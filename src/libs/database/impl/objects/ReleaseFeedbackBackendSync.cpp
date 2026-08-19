/*
 * Copyright (C) 2026 Emeric Poupon
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

#include "database/objects/ReleaseFeedbackBackendSync.hpp"

#include <Wt/Dbo/Impl.h>
#include <Wt/Dbo/WtSqlTraits.h>

#include "database/Session.hpp"
#include "database/objects/ReleaseFeedback.hpp"

#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::ReleaseFeedbackBackendSync)

namespace lms::db
{
    ReleaseFeedbackBackendSync::ReleaseFeedbackBackendSync(ObjectPtr<ReleaseFeedback> releaseFeedback, FeedbackBackend backend)
        : _backend{ backend }
        , _releaseFeedback{ getDboPtr(releaseFeedback) }
    {
    }

    ReleaseFeedbackBackendSync::pointer ReleaseFeedbackBackendSync::create(Session& session, ObjectPtr<ReleaseFeedback> releaseFeedback, FeedbackBackend backend)
    {
        session.checkWriteTransaction();
        return session.getDboSession()->add(std::unique_ptr<ReleaseFeedbackBackendSync>{ new ReleaseFeedbackBackendSync{ releaseFeedback, backend } });
    }

    std::size_t ReleaseFeedbackBackendSync::getCount(Session& session)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(*) FROM release_feedback_backend_sync"));
    }

    ReleaseFeedbackBackendSync::pointer ReleaseFeedbackBackendSync::find(Session& session, ReleaseFeedbackBackendSyncId id)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->find<ReleaseFeedbackBackendSync>().where("id = ?").bind(id));
    }

    ReleaseFeedbackBackendSync::pointer ReleaseFeedbackBackendSync::find(Session& session, ReleaseFeedbackId releaseFeedbackId, FeedbackBackend backend)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->find<ReleaseFeedbackBackendSync>().where("release_feedback_id = ?").bind(releaseFeedbackId).where("backend = ?").bind(backend));
    }

    std::vector<ReleaseFeedbackBackendSyncId> ReleaseFeedbackBackendSync::find(Session& session, const FindParameters& params)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<ReleaseFeedbackBackendSyncId>("SELECT id FROM release_feedback_backend_sync") };

        if (params.backend)
            query.where("backend = ?").bind(*params.backend);

        if (params.syncState)
            query.where("sync_state = ?").bind(*params.syncState);

        return utils::execRangeQuery<ReleaseFeedbackBackendSyncId>(query, params.range);
    }

    void ReleaseFeedbackBackendSync::find(Session& session, const FindParameters& params, const std::function<void(const pointer&)>& func)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->find<ReleaseFeedbackBackendSync>() };

        if (params.backend)
            query.where("backend = ?").bind(*params.backend);

        if (params.syncState)
            query.where("sync_state = ?").bind(*params.syncState);

        utils::forEachQueryRangeResult(query, params.range, func);
    }
} // namespace lms::db
