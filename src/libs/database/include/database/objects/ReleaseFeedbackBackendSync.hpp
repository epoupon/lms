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

#include "database/Object.hpp"
#include "database/Types.hpp"
#include "database/objects/ReleaseFeedbackBackendSyncId.hpp"
#include "database/objects/ReleaseFeedbackId.hpp"
#include "database/objects/Types.hpp"

namespace lms::db
{
    class ISession;
    class ReleaseFeedback;

    // Tracks the delivery of a single ReleaseFeedback to a given external feedback backend.
    class ReleaseFeedbackBackendSync : public Object<ReleaseFeedbackBackendSync, ReleaseFeedbackBackendSyncId>
    {
    public:
        static pointer create(ISession& session, const ObjectPtr<ReleaseFeedback>& releaseFeedback, FeedbackBackend backend);

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

        static std::size_t getCount(ISession& session);
        static pointer find(ISession& session, ReleaseFeedbackBackendSyncId id);
        static pointer find(ISession& session, ReleaseFeedbackId releaseFeedbackId, FeedbackBackend backend);
        static std::vector<ReleaseFeedbackBackendSyncId> find(ISession& session, const FindParameters& params);
        static void find(ISession& session, const FindParameters& params, const std::function<void(const pointer&)>& func);

        ObjectPtr<ReleaseFeedback> getReleaseFeedback() const;
        FeedbackBackend getBackend() const { return _backend; }
        SyncState getSyncState() const { return _syncState; }

        void setSyncState(SyncState state) { _syncState = state; }

    protected:
        ReleaseFeedbackBackendSync() = default;
        explicit ReleaseFeedbackBackendSync(FeedbackBackend backend);

        FeedbackBackend _backend;
        SyncState _syncState{ SyncState::PendingAdd };
    };
} // namespace lms::db
