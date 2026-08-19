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

#include "database/Session.hpp"
#include "database/objects/Listen.hpp"

#include "Utils.hpp"
#include "traits/IdTypeTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::ListenBackendSync)

namespace lms::db
{
    ListenBackendSync::ListenBackendSync(ObjectPtr<Listen> listen, ScrobblingBackend backend)
        : _backend{ backend }
        , _listen{ getDboPtr(listen) }
    {
    }

    ListenBackendSync::pointer ListenBackendSync::create(Session& session, ObjectPtr<Listen> listen, ScrobblingBackend backend)
    {
        session.checkWriteTransaction();
        return session.getDboSession()->add(std::unique_ptr<ListenBackendSync>{ new ListenBackendSync{ listen, backend } });
    }

    std::size_t ListenBackendSync::getCount(Session& session)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->query<int>("SELECT COUNT(*) FROM listen_backend_sync"));
    }

    ListenBackendSync::pointer ListenBackendSync::find(Session& session, ListenBackendSyncId id)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->find<ListenBackendSync>().where("id = ?").bind(id));
    }

    ListenBackendSync::pointer ListenBackendSync::find(Session& session, ListenId listenId, ScrobblingBackend backend)
    {
        session.checkReadTransaction();
        return utils::fetchQuerySingleResult(session.getDboSession()->find<ListenBackendSync>().where("listen_id = ?").bind(listenId).where("backend = ?").bind(backend));
    }

    std::vector<ListenBackendSyncId> ListenBackendSync::find(Session& session, const FindParameters& params)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->query<ListenBackendSyncId>("SELECT id FROM listen_backend_sync") };

        if (params.backend)
            query.where("backend = ?").bind(*params.backend);

        if (params.syncState)
            query.where("sync_state = ?").bind(*params.syncState);

        return utils::execRangeQuery<ListenBackendSyncId>(query, params.range);
    }

    void ListenBackendSync::find(Session& session, const FindParameters& params, const std::function<void(const pointer&)>& func)
    {
        session.checkReadTransaction();

        auto query{ session.getDboSession()->find<ListenBackendSync>() };

        if (params.backend)
            query.where("backend = ?").bind(*params.backend);

        if (params.syncState)
            query.where("sync_state = ?").bind(*params.syncState);

        utils::forEachQueryRangeResult(query, params.range, func);
    }
} // namespace lms::db
