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

#include "database/objects/ListenBackendSync.hpp"

#include <Wt/Dbo/Impl.h>
#include <Wt/Dbo/WtSqlTraits.h>

#include "DboAccess.hpp"
#include "ListenBackendSyncDbo.hpp"
#include "ListenDbo.hpp"
#include "Session.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::ListenBackendSync)

namespace lms::db
{
    ListenBackendSync::ListenBackendSync(ScrobblingBackend backend)
        : _backend{ backend }
    {
    }

    dbo::ListenBackendSync::ListenBackendSync(Wt::Dbo::ptr<Listen> listen, ScrobblingBackend backend)
        : db::ListenBackendSync{ backend }
        , _listen{ std::move(listen) }
    {
    }

    ListenBackendSync ListenBackendSync::create(ISession& session, ListenId listen, ScrobblingBackend backend)
    {
        return utils::addObject(session, std::unique_ptr<dbo::ListenBackendSync>{ new dbo::ListenBackendSync{ getDboSession(session)->loadLazy<dbo::Listen>(listen.getValue()), backend } });
    }

    void ListenBackendSync::destroy(ISession& session, std::span<const ListenBackendSyncId> ids)
    {
        utils::destroyObjects<dbo::ListenBackendSync>(session, ids);
    }

    std::size_t ListenBackendSync::getCount(ISession& session)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM listen_backend_sync"));
    }

    std::optional<ListenBackendSync> ListenBackendSync::find(ISession& session, ListenBackendSyncId id)
    {
        checkReadTransaction(session);

        if (!id.isValid())
            return std::nullopt;

        return utils::fetchQuerySingleObject(getDboSession(session)->find<dbo::ListenBackendSync>().where("id = ?").bind(id));
    }

    std::optional<ListenBackendSync> ListenBackendSync::find(ISession& session, ListenId listenId, ScrobblingBackend backend)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleObject(getDboSession(session)->find<dbo::ListenBackendSync>().where("listen_id = ?").bind(listenId).where("backend = ?").bind(backend));
    }

    std::vector<ListenBackendSyncId> ListenBackendSync::findIds(ISession& session, const FindParameters& params)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<ListenBackendSyncId>("SELECT id FROM listen_backend_sync") };

        if (params.backend)
            query.where("backend = ?").bind(*params.backend);

        if (params.syncState)
            query.where("sync_state = ?").bind(*params.syncState);

        return utils::execRangeQuery<ListenBackendSyncId>(query, params.range);
    }

    void ListenBackendSync::find(ISession& session, const FindParameters& params, const std::function<void(const ListenBackendSync&)>& func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->find<dbo::ListenBackendSync>() };

        if (params.backend)
            query.where("backend = ?").bind(*params.backend);

        if (params.syncState)
            query.where("sync_state = ?").bind(*params.syncState);

        utils::forEachQueryRangeObject(query, params.range, func);
    }

    void ListenBackendSync::update(ISession& session, const ListenBackendSync& listenBackendSync)
    {
        utils::updateObject<dbo::ListenBackendSync>(session, listenBackendSync);
    }
} // namespace lms::db
