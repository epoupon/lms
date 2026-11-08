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

#include "database/objects/ServerInfo.hpp"

#include <Wt/Dbo/Impl.h>
#include <Wt/Dbo/WtSqlTraits.h>

#include "ObjectPtrImpl.hpp"
#include "ServerInfoDbo.hpp"
#include "Session.hpp"
#include "Utils.hpp"
#include "traits/UUIDTraits.hpp"

LMS_DB_INSTANTIATE_OBJECT(lms::db::ServerInfo)

namespace lms::db
{
    ServerInfo::ServerInfo(core::UUID instanceId)
        : _instanceId{ instanceId }
    {
    }

    ServerInfo::pointer ServerInfo::getOrCreate(ISession& session)
    {
        checkWriteTransaction(session);

        pointer serverInfo{ utils::fetchQuerySingleResult(getDboSession(session)->find<dbo::ServerInfo>()) };
        if (!serverInfo)
            return getDboSession(session)->add(std::unique_ptr<dbo::ServerInfo>{ new dbo::ServerInfo{ core::UUID::generate() } });

        return serverInfo;
    }

    ServerInfo::pointer ServerInfo::get(ISession& session)
    {
        checkReadTransaction(session);

        return utils::fetchQuerySingleResult(getDboSession(session)->find<dbo::ServerInfo>());
    }
} // namespace lms::db
