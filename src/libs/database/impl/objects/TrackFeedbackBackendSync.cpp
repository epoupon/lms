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

#include "database/objects/TrackFeedbackBackendSync.hpp"

#include <Wt/Dbo/Impl.h>
#include <Wt/Dbo/WtSqlTraits.h>

#include "database/objects/TrackFeedback.hpp"

#include "Session.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::TrackFeedbackBackendSync)

namespace lms::db
{
    TrackFeedbackBackendSync::TrackFeedbackBackendSync(const ObjectPtr<TrackFeedback>& trackFeedback, FeedbackBackend backend)
        : _backend{ backend }
        , _trackFeedback{ getDboPtr(trackFeedback) }
    {
    }

    TrackFeedbackBackendSync::pointer TrackFeedbackBackendSync::create(ISession& session, const ObjectPtr<TrackFeedback>& trackFeedback, FeedbackBackend backend)
    {
        return utils::addObject(session, std::unique_ptr<TrackFeedbackBackendSync>{ new TrackFeedbackBackendSync{ trackFeedback, backend } });
    }

    std::size_t TrackFeedbackBackendSync::getCount(ISession& session)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM track_feedback_backend_sync"));
    }

    TrackFeedbackBackendSync::pointer TrackFeedbackBackendSync::find(ISession& session, TrackFeedbackBackendSyncId id)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->find<TrackFeedbackBackendSync>().where("id = ?").bind(id));
    }

    TrackFeedbackBackendSync::pointer TrackFeedbackBackendSync::find(ISession& session, TrackFeedbackId trackFeedbackId, FeedbackBackend backend)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->find<TrackFeedbackBackendSync>().where("track_feedback_id = ?").bind(trackFeedbackId).where("backend = ?").bind(backend));
    }

    std::vector<TrackFeedbackBackendSyncId> TrackFeedbackBackendSync::find(ISession& session, const FindParameters& params)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<TrackFeedbackBackendSyncId>("SELECT id FROM track_feedback_backend_sync") };

        if (params.backend)
            query.where("backend = ?").bind(*params.backend);

        if (params.syncState)
            query.where("sync_state = ?").bind(*params.syncState);

        return utils::execRangeQuery<TrackFeedbackBackendSyncId>(query, params.range);
    }

    void TrackFeedbackBackendSync::find(ISession& session, const FindParameters& params, const std::function<void(const pointer&)>& func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->find<TrackFeedbackBackendSync>() };

        if (params.backend)
            query.where("backend = ?").bind(*params.backend);

        if (params.syncState)
            query.where("sync_state = ?").bind(*params.syncState);

        utils::forEachQueryRangeResult(query, params.range, func);
    }
} // namespace lms::db
