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

#include "database/objects/TrackFeedbackBackendSync.hpp"

#include "DboAccess.hpp"

namespace lms::db::dbo
{
    class TrackFeedback;

    class TrackFeedbackBackendSync final : public db::TrackFeedbackBackendSync, public Wt::Dbo::Dbo<TrackFeedbackBackendSync>
    {
    public:
        TrackFeedbackBackendSync() = default;
        using db::TrackFeedbackBackendSync::TrackFeedbackBackendSync;

        TrackFeedbackBackendSync(Wt::Dbo::ptr<TrackFeedback> trackFeedback, FeedbackBackend backend);

        void syncIds()
        {
            _id = id();
            _trackFeedbackId = _trackFeedback.id();
        }

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _backend, "backend");
            Wt::Dbo::field(a, _syncState, "sync_state");

            Wt::Dbo::belongsTo(a, _trackFeedback, "track_feedback", Wt::Dbo::OnDeleteCascade);
        }

        Wt::Dbo::ptr<TrackFeedback> _trackFeedback;
    };
} // namespace lms::db::dbo

namespace lms::db
{
    template<>
    struct DboOf<TrackFeedbackBackendSync>
    {
        using type = dbo::TrackFeedbackBackendSync;
    };
} // namespace lms::db
