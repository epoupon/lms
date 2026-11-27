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
#include "DboAccess.hpp"
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

    dbo::ArtistFeedbackBackendSync::ArtistFeedbackBackendSync(Wt::Dbo::ptr<ArtistFeedback> artistFeedback, FeedbackBackend backend)
        : db::ArtistFeedbackBackendSync{ backend }
        , _artistFeedback{ std::move(artistFeedback) }
    {
    }

    ArtistFeedbackBackendSync ArtistFeedbackBackendSync::create(ISession& session, ArtistFeedbackId artistFeedback, FeedbackBackend backend)
    {
        return utils::addObject(session, std::unique_ptr<dbo::ArtistFeedbackBackendSync>{ new dbo::ArtistFeedbackBackendSync{ getDboSession(session)->loadLazy<dbo::ArtistFeedback>(artistFeedback.getValue()), backend } });
    }

    void ArtistFeedbackBackendSync::destroy(ISession& session, std::span<const ArtistFeedbackBackendSyncId> ids)
    {
        utils::destroyObjects<dbo::ArtistFeedbackBackendSync>(session, ids);
    }

    std::size_t ArtistFeedbackBackendSync::getCount(ISession& session)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM artist_feedback_backend_sync"));
    }

    std::optional<ArtistFeedbackBackendSync> ArtistFeedbackBackendSync::find(ISession& session, ArtistFeedbackBackendSyncId id)
    {
        checkReadTransaction(session);

        if (!id.isValid())
            return std::nullopt;

        return utils::fetchQuerySingleObject(getDboSession(session)->find<dbo::ArtistFeedbackBackendSync>().where("id = ?").bind(id));
    }

    std::optional<ArtistFeedbackBackendSync> ArtistFeedbackBackendSync::find(ISession& session, ArtistFeedbackId artistFeedbackId, FeedbackBackend backend)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleObject(getDboSession(session)->find<dbo::ArtistFeedbackBackendSync>().where("artist_feedback_id = ?").bind(artistFeedbackId).where("backend = ?").bind(backend));
    }

    void ArtistFeedbackBackendSync::find(ISession& session, const FindParameters& params, const std::function<void(const ArtistFeedbackBackendSync&)>& func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->find<dbo::ArtistFeedbackBackendSync>() };

        if (params.backend)
            query.where("backend = ?").bind(*params.backend);

        if (params.syncState)
            query.where("sync_state = ?").bind(*params.syncState);

        utils::forEachQueryRangeObject(query, params.range, func);
    }

    void ArtistFeedbackBackendSync::update(ISession& session, const ArtistFeedbackBackendSync& artistFeedbackBackendSync)
    {
        utils::updateObject<dbo::ArtistFeedbackBackendSync>(session, artistFeedbackBackendSync);
    }
} // namespace lms::db
