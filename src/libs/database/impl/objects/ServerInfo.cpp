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

#include "database/Session.hpp"

#include "Utils.hpp"
#include "traits/UUIDTraits.hpp"

DBO_INSTANTIATE_TEMPLATES(lms::db::ServerInfo)

namespace lms::db
{
    ServerInfo::ServerInfo(core::UUID instanceId)
        : _instanceId{ instanceId }
    {
    }

    ServerInfo::pointer ServerInfo::getOrCreate(Session& session)
    {
        session.checkWriteTransaction();

        pointer serverInfo{ utils::fetchQuerySingleResult(session.getDboSession()->find<ServerInfo>()) };
        if (!serverInfo)
            return session.getDboSession()->add(std::unique_ptr<ServerInfo>{ new ServerInfo{ core::UUID::generate() } });

        return serverInfo;
    }

    ServerInfo::pointer ServerInfo::get(Session& session)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->find<ServerInfo>());
    }
} // namespace lms::db
