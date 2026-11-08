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

#include "database/objects/ArtistFeedbackBackendSync.hpp"

#include <Wt/Dbo/Impl.h>
#include <Wt/Dbo/WtSqlTraits.h>

#include "ArtistFeedbackBackendSyncDbo.hpp"
#include "ArtistFeedbackDbo.hpp"
#include "ObjectPtrImpl.hpp"
#include "Session.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::ArtistFeedbackBackendSync)

namespace lms::db
{
    ArtistFeedbackBackendSync::ArtistFeedbackBackendSync(FeedbackBackend backend)
        : _backend{ backend }
    {
    }

    dbo::ArtistFeedbackBackendSync::ArtistFeedbackBackendSync(const ObjectPtr<db::ArtistFeedback>& artistFeedback, FeedbackBackend backend)
        : db::ArtistFeedbackBackendSync{ backend }
        , _artistFeedback{ getDboPtr(artistFeedback) }
    {
    }

    ArtistFeedbackBackendSync::pointer ArtistFeedbackBackendSync::create(ISession& session, const ObjectPtr<ArtistFeedback>& artistFeedback, FeedbackBackend backend)
    {
        return utils::addObject(session, std::unique_ptr<dbo::ArtistFeedbackBackendSync>{ new dbo::ArtistFeedbackBackendSync{ artistFeedback, backend } });
    }

    std::size_t ArtistFeedbackBackendSync::getCount(ISession& session)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM artist_feedback_backend_sync"));
    }

    ArtistFeedbackBackendSync::pointer ArtistFeedbackBackendSync::find(ISession& session, ArtistFeedbackBackendSyncId id)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->find<dbo::ArtistFeedbackBackendSync>().where("id = ?").bind(id));
    }

    ArtistFeedbackBackendSync::pointer ArtistFeedbackBackendSync::find(ISession& session, ArtistFeedbackId artistFeedbackId, FeedbackBackend backend)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->find<dbo::ArtistFeedbackBackendSync>().where("artist_feedback_id = ?").bind(artistFeedbackId).where("backend = ?").bind(backend));
    }

    std::vector<ArtistFeedbackBackendSyncId> ArtistFeedbackBackendSync::find(ISession& session, const FindParameters& params)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<ArtistFeedbackBackendSyncId>("SELECT id FROM artist_feedback_backend_sync") };

        if (params.backend)
            query.where("backend = ?").bind(*params.backend);

        if (params.syncState)
            query.where("sync_state = ?").bind(*params.syncState);

        return utils::execRangeQuery<ArtistFeedbackBackendSyncId>(query, params.range);
    }

    void ArtistFeedbackBackendSync::find(ISession& session, const FindParameters& params, const std::function<void(const pointer&)>& func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->find<dbo::ArtistFeedbackBackendSync>() };

        if (params.backend)
            query.where("backend = ?").bind(*params.backend);

        if (params.syncState)
            query.where("sync_state = ?").bind(*params.syncState);

        utils::forEachQueryRangeResult(query, params.range, func);
    }

    ObjectPtr<ArtistFeedback> ArtistFeedbackBackendSync::getArtistFeedback() const
    {
        return getDbo(*this)._artistFeedback;
    }
} // namespace lms::db
