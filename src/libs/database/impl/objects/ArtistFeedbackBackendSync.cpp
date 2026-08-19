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

#include "database/Session.hpp"
#include "database/objects/ArtistFeedback.hpp"

#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::ArtistFeedbackBackendSync)

namespace lms::db
{
    ArtistFeedbackBackendSync::ArtistFeedbackBackendSync(ObjectPtr<ArtistFeedback> artistFeedback, FeedbackBackend backend)
        : _backend{ backend }
        , _artistFeedback{ getDboPtr(artistFeedback) }
    {
    }

    ArtistFeedbackBackendSync::pointer ArtistFeedbackBackendSync::create(Session& session, ObjectPtr<ArtistFeedback> artistFeedback, FeedbackBackend backend)
    {
        session.checkWriteTransaction();
        return session.getDboSession()->add(std::unique_ptr<ArtistFeedbackBackendSync>{ new ArtistFeedbackBackendSync{ artistFeedback, backend } });
    }

    std::size_t ArtistFeedbackBackendSync::getCount(Session& session)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(*) FROM artist_feedback_backend_sync"));
    }

    ArtistFeedbackBackendSync::pointer ArtistFeedbackBackendSync::find(Session& session, ArtistFeedbackBackendSyncId id)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->find<ArtistFeedbackBackendSync>().where("id = ?").bind(id));
    }

    ArtistFeedbackBackendSync::pointer ArtistFeedbackBackendSync::find(Session& session, ArtistFeedbackId artistFeedbackId, FeedbackBackend backend)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->find<ArtistFeedbackBackendSync>().where("artist_feedback_id = ?").bind(artistFeedbackId).where("backend = ?").bind(backend));
    }

    std::vector<ArtistFeedbackBackendSyncId> ArtistFeedbackBackendSync::find(Session& session, const FindParameters& params)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<ArtistFeedbackBackendSyncId>("SELECT id FROM artist_feedback_backend_sync") };

        if (params.backend)
            query.where("backend = ?").bind(*params.backend);

        if (params.syncState)
            query.where("sync_state = ?").bind(*params.syncState);

        return utils::execRangeQuery<ArtistFeedbackBackendSyncId>(query, params.range);
    }

    void ArtistFeedbackBackendSync::find(Session& session, const FindParameters& params, const std::function<void(const pointer&)>& func)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->find<ArtistFeedbackBackendSync>() };

        if (params.backend)
            query.where("backend = ?").bind(*params.backend);

        if (params.syncState)
            query.where("sync_state = ?").bind(*params.syncState);

        utils::forEachQueryRangeResult(query, params.range, func);
    }
} // namespace lms::db
