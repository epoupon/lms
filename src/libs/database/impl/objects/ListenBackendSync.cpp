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

#include "database/objects/Listen.hpp"

#include "ObjectPtrImpl.hpp"
#include "Session.hpp"
#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::ListenBackendSync)

namespace lms::db
{
    ListenBackendSync::ListenBackendSync(const ObjectPtr<Listen>& listen, ScrobblingBackend backend)
        : _backend{ backend }
        , _listen{ getDboPtr(listen) }
    {
    }

    ListenBackendSync::pointer ListenBackendSync::create(ISession& session, const ObjectPtr<Listen>& listen, ScrobblingBackend backend)
    {
        return utils::addObject(session, std::unique_ptr<ListenBackendSync>{ new ListenBackendSync{ listen, backend } });
    }

    std::size_t ListenBackendSync::getCount(ISession& session)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->query<int>("SELECT COUNT(*) FROM listen_backend_sync"));
    }

    ListenBackendSync::pointer ListenBackendSync::find(ISession& session, ListenBackendSyncId id)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->find<ListenBackendSync>().where("id = ?").bind(id));
    }

    ListenBackendSync::pointer ListenBackendSync::find(ISession& session, ListenId listenId, ScrobblingBackend backend)
    {
        checkReadTransaction(session);
        return utils::fetchQuerySingleResult(getDboSession(session)->find<ListenBackendSync>().where("listen_id = ?").bind(listenId).where("backend = ?").bind(backend));
    }

    std::vector<ListenBackendSyncId> ListenBackendSync::find(ISession& session, const FindParameters& params)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->query<ListenBackendSyncId>("SELECT id FROM listen_backend_sync") };

        if (params.backend)
            query.where("backend = ?").bind(*params.backend);

        if (params.syncState)
            query.where("sync_state = ?").bind(*params.syncState);

        return utils::execRangeQuery<ListenBackendSyncId>(query, params.range);
    }

    void ListenBackendSync::find(ISession& session, const FindParameters& params, const std::function<void(const pointer&)>& func)
    {
        checkReadTransaction(session);

        auto query{ getDboSession(session)->find<ListenBackendSync>() };

        if (params.backend)
            query.where("backend = ?").bind(*params.backend);

        if (params.syncState)
            query.where("sync_state = ?").bind(*params.syncState);

        utils::forEachQueryRangeResult(query, params.range, func);
    }
} // namespace lms::db
