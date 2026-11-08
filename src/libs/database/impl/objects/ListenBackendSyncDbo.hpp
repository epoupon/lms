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

#include <Wt/Dbo/Dbo.h>
#include <Wt/Dbo/Field.h>

#include "database/objects/ListenBackendSync.hpp"

#include "ObjectPtrImpl.hpp"

namespace lms::db::dbo
{
    class Listen;

    class ListenBackendSync final : public db::ListenBackendSync, public Wt::Dbo::Dbo<ListenBackendSync>
    {
    public:
        ListenBackendSync() = default;
        using db::ListenBackendSync::ListenBackendSync;

        ListenBackendSync(const ObjectPtr<db::Listen>& listen, ScrobblingBackend backend);

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _backend, "backend");
            Wt::Dbo::field(a, _syncState, "sync_state");

            Wt::Dbo::belongsTo(a, _listen, "listen", Wt::Dbo::OnDeleteCascade);
        }

        Wt::Dbo::ptr<Listen> _listen;
    };
} // namespace lms::db::dbo

namespace lms::db
{
    template<>
    struct DboOf<ListenBackendSync>
    {
        using type = dbo::ListenBackendSync;
    };
} // namespace lms::db
