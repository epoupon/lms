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

#include "DboAccess.hpp"
#include "Session.hpp"
#include "TrackFeedbackBackendSyncDbo.hpp"
#include "TrackFeedbackDbo.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::TrackFeedbackBackendSync)

namespace lms::db
{
    TrackFeedbackBackendSync::TrackFeedbackBackendSync(FeedbackBackend backend)
        : _backend{ backend }
    {
    }

    dbo::TrackFeedbackBackendSync::TrackFeedbackBackendSync(Wt::Dbo::ptr<TrackFeedback> trackFeedback, FeedbackBackend backend)
        : db::TrackFeedbackBackendSync{ backend }
        , _trackFeedback{ std::move(trackFeedback) }
    {
    }

    TrackFeedbackBackendSync TrackFeedbackBackendSync::create(ISession& session, TrackFeedbackId trackFeedback, FeedbackBackend backend)
    {
        return utils::addObject(session, std::unique_ptr<dbo::TrackFeedbackBackendSync>{ new dbo::TrackFeedbackBackendSync{ getDboSession(session)->loadLazy<dbo::TrackFeedback>(trackFeedback.getValue()), backend } });
    }

    void TrackFeedbackBackendSync::destroy(ISession& session, std::span<const TrackFeedbackBackendSyncId> ids)
    {
        utils::destroyObjects<dbo::TrackFeedbackBackendSync>(session, ids);
    }

    std::size_t TrackFeedbackBackendSync::getCount(ISession& session)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM track_feedback_backend_sync"));
    }

    std::optional<TrackFeedbackBackendSync> TrackFeedbackBackendSync::find(ISession& session, TrackFeedbackBackendSyncId id)
    {
        checkReadTransaction(session);

        if (!id.isValid())
            return std::nullopt;

        return utils::fetchQuerySingleObject(getDboSession(session)->find<dbo::TrackFeedbackBackendSync>().where("id = ?").bind(id));
    }

    std::optional<TrackFeedbackBackendSync> TrackFeedbackBackendSync::find(ISession& session, TrackFeedbackId trackFeedbackId, FeedbackBackend backend)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleObject(getDboSession(session)->find<dbo::TrackFeedbackBackendSync>().where("track_feedback_id = ?").bind(trackFeedbackId).where("backend = ?").bind(backend));
    }

    void TrackFeedbackBackendSync::find(ISession& session, const FindParameters& params, const std::function<void(const TrackFeedbackBackendSync&)>& func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->find<dbo::TrackFeedbackBackendSync>() };

        if (params.backend)
            query.where("backend = ?").bind(*params.backend);

        if (params.syncState)
            query.where("sync_state = ?").bind(*params.syncState);

        utils::forEachQueryRangeObject(query, params.range, func);
    }

    void TrackFeedbackBackendSync::update(ISession& session, const TrackFeedbackBackendSync& trackFeedbackBackendSync)
    {
        utils::updateObject<dbo::TrackFeedbackBackendSync>(session, trackFeedbackBackendSync);
    }
} // namespace lms::db
