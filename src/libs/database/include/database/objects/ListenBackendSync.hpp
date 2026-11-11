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
#include <span>

#include "database/Types.hpp"
#include "database/objects/ListenBackendSyncId.hpp"
#include "database/objects/ListenId.hpp"
#include "database/objects/Types.hpp"
#include "database/objects/detail/Object.hpp"

namespace lms::db
{
    class ISession;
    class Listen;

    // Tracks the delivery of a single Listen to a given external scrobbling backend.
    // Never created for local/internal recording: recording a Listen row is itself that behavior.
    class ListenBackendSync : public detail::Object<ListenBackendSync, ListenBackendSyncId>
    {
    public:
        static ListenBackendSync create(ISession& session, ListenId listen, ScrobblingBackend backend);
        static void destroy(ISession& session, std::span<const ListenBackendSyncId> ids);

        struct FindParameters
        {
            std::optional<ScrobblingBackend> backend;
            std::optional<SyncState> syncState;
            std::optional<Range> range;

            FindParameters& setBackend(ScrobblingBackend _backend)
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
        static std::optional<ListenBackendSync> find(ISession& session, ListenBackendSyncId id);
        static std::optional<ListenBackendSync> find(ISession& session, ListenId listenId, ScrobblingBackend backend);
        static std::vector<ListenBackendSyncId> findIds(ISession& session, const FindParameters& params);
        static void find(ISession& session, const FindParameters& params, const std::function<void(const ListenBackendSync&)>& func);

        static void update(ISession& session, const ListenBackendSync& listenBackendSync);

        ListenId getListenId() const { return _listenId; }
        ScrobblingBackend getBackend() const { return _backend; }
        SyncState getSyncState() const { return _syncState; }

        void setSyncState(SyncState state) { _syncState = state; }

    protected:
        ListenBackendSync() = default;
        explicit ListenBackendSync(ScrobblingBackend backend);

        ScrobblingBackend _backend;
        SyncState _syncState{ SyncState::PendingAdd };
        ListenId _listenId;
    };
} // namespace lms::db
