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

#include <functional>
#include <optional>

#include <Wt/Dbo/Field.h>

#include "database/Object.hpp"
#include "database/Types.hpp"
#include "database/objects/ArtistFeedbackBackendSyncId.hpp"
#include "database/objects/ArtistFeedbackId.hpp"
#include "database/objects/Types.hpp"

namespace lms::db
{
    class ArtistFeedback;
    class Session;

    // Tracks the delivery of a single ArtistFeedback to a given external feedback backend.
    class ArtistFeedbackBackendSync final : public Object<ArtistFeedbackBackendSync, ArtistFeedbackBackendSyncId>
    {
    public:
        ArtistFeedbackBackendSync() = default;

        struct FindParameters
        {
            std::optional<FeedbackBackend> backend;
            std::optional<SyncState> syncState;
            std::optional<Range> range;

            FindParameters& setBackend(FeedbackBackend _backend)
            {
                backend = _backend;
                return *this;
            }
            FindParameters& setSyncState(SyncState _syncState)
            {
                syncState = _syncState;
                return *this;
            }
            FindParameters& setRange(std::optional<Range> _range)
            {
                range = _range;
                return *this;
            }
        };

        static std::size_t getCount(Session& session);
        static pointer find(Session& session, ArtistFeedbackBackendSyncId id);
        static pointer find(Session& session, ArtistFeedbackId artistFeedbackId, FeedbackBackend backend);
        static std::vector<ArtistFeedbackBackendSyncId> find(Session& session, const FindParameters& params);
        static void find(Session& session, const FindParameters& params, const std::function<void(const pointer&)>& func);

        ObjectPtr<ArtistFeedback> getArtistFeedback() const { return _artistFeedback; }
        FeedbackBackend getBackend() const { return _backend; }
        SyncState getSyncState() const { return _syncState; }

        void setSyncState(SyncState state) { _syncState = state; }

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _backend, "backend");
            Wt::Dbo::field(a, _syncState, "sync_state");

            Wt::Dbo::belongsTo(a, _artistFeedback, "artist_feedback", Wt::Dbo::OnDeleteCascade);
        }

    private:
        friend class Session;
        ArtistFeedbackBackendSync(ObjectPtr<ArtistFeedback> artistFeedback, FeedbackBackend backend);
        static pointer create(Session& session, ObjectPtr<ArtistFeedback> artistFeedback, FeedbackBackend backend);

        FeedbackBackend _backend;
        SyncState _syncState{ SyncState::PendingAdd };

        Wt::Dbo::ptr<ArtistFeedback> _artistFeedback;
    };
} // namespace lms::db
