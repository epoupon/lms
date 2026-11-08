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

#pragma once

#include "core/UUID.hpp"
#include "database/Object.hpp"
#include "database/objects/ServerInfoId.hpp"

namespace lms::db
{
    class ISession;

    // Singleton row holding server-level metadata (not tied to any particular schema version)
    class ServerInfo : public Object<ServerInfo, ServerInfoId>
    {
    public:
        static pointer getOrCreate(ISession& session);
        static pointer get(ISession& session);

        core::UUID getInstanceId() const { return _instanceId; }

    protected:
        ServerInfo() = default;
        explicit ServerInfo(core::UUID instanceId);

        core::UUID _instanceId;
    };
} // namespace lms::db
