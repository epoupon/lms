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

#include <Wt/Dbo/Field.h>

#include "core/UUID.hpp"

namespace lms::db
{
    class Session;

    // Singleton row holding server-level metadata (not tied to any particular schema version)
    class ServerInfo
    {
    public:
        using pointer = Wt::Dbo::ptr<ServerInfo>;

        ServerInfo() = default;

        static pointer getOrCreate(Session& session);
        static pointer get(Session& session);

        core::UUID getInstanceId() const { return _instanceId; }

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _instanceId, "instance_id");
        }

    private:
        explicit ServerInfo(core::UUID instanceId);

        core::UUID _instanceId;
    };
} // namespace lms::db
