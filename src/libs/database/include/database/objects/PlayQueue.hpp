/*
 * Copyright (C) 2024 Emeric Poupon
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

#include <chrono>
#include <optional>
#include <span>
#include <vector>

#include <Wt/WDateTime.h>

#include "database/objects/PlayQueueId.hpp"
#include "database/objects/TrackListId.hpp"
#include "database/objects/Types.hpp"
#include "database/objects/UserId.hpp"
#include "database/objects/detail/Object.hpp"

namespace lms::db
{
    class ISession;

    // PlayQueue is a tracklist + a cursor on an internal track list
    class PlayQueue : public detail::Object<PlayQueue, PlayQueueId>
    {
    public:
        static PlayQueue create(ISession& session, UserId user, PlayQueueType type);
        static void destroy(ISession& session, std::span<const PlayQueueId> ids);

        static std::size_t getCount(ISession& session);
        static std::optional<PlayQueue> find(ISession& session, PlayQueueId playQueueId);
        static std::optional<PlayQueue> find(ISession& session, UserId userId, PlayQueueType type);
        static std::vector<PlayQueueId> findIds(ISession& session, UserId userId);

        static void update(ISession& session, const PlayQueue& playQueue);

        // Accessors
        PlayQueueType getType() const { return _type; }
        TrackListId getTrackListId() const { return _trackListId; }
        std::size_t getCurrentIndex() const { return _currentIndex; }
        std::chrono::milliseconds getCurrentPositionInTrack() const { return _currentPositionInTrack; }
        UserId getUserId() const { return _userId; }
        Wt::WDateTime getLastModifiedDateTime() const { return _lastModifiedDateTime; }

        // Modifiers
        void setCurrentIndex(std::size_t index) { _currentIndex = index; }
        void setCurrentPositionInTrack(std::chrono::milliseconds position) { _currentPositionInTrack = position; }
        void setLastModifiedDateTime(const Wt::WDateTime& lastModified) { _lastModifiedDateTime = lastModified; }

    protected:
        PlayQueue() = default;
        explicit PlayQueue(PlayQueueType type);

        PlayQueueType _type{ PlayQueueType::UI };
        int _currentIndex{};
        std::chrono::duration<int, std::milli> _currentPositionInTrack{};
        Wt::WDateTime _lastModifiedDateTime;
        UserId _userId;
        TrackListId _trackListId;
    };
} // namespace lms::db
