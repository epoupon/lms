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

#include "database/objects/ReleaseFeedbackBackendSync.hpp"

#include "DboAccess.hpp"

namespace lms::db::dbo
{
    class ReleaseFeedback;

    class ReleaseFeedbackBackendSync final : public db::ReleaseFeedbackBackendSync, public Wt::Dbo::Dbo<ReleaseFeedbackBackendSync>
    {
    public:
        ReleaseFeedbackBackendSync() = default;
        using db::ReleaseFeedbackBackendSync::ReleaseFeedbackBackendSync;

        ReleaseFeedbackBackendSync(Wt::Dbo::ptr<ReleaseFeedback> releaseFeedback, FeedbackBackend backend);

        void syncIds()
        {
            _id = id();
            _releaseFeedbackId = _releaseFeedback.id();
        }

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _backend, "backend");
            Wt::Dbo::field(a, _syncState, "sync_state");

            Wt::Dbo::belongsTo(a, _releaseFeedback, "release_feedback", Wt::Dbo::OnDeleteCascade);
        }

        Wt::Dbo::ptr<ReleaseFeedback> _releaseFeedback;
    };
} // namespace lms::db::dbo

namespace lms::db
{
    template<>
    struct DboOf<ReleaseFeedbackBackendSync>
    {
        using type = dbo::ReleaseFeedbackBackendSync;
    };
} // namespace lms::db
