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
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <Wt/WDateTime.h>

#include "database/objects/PlayQueueId.hpp"
#include "database/objects/TrackId.hpp"
#include "database/objects/UserId.hpp"
#include "database/objects/detail/Object.hpp"

namespace lms::db
{
    class ISession;
    class User;

    // Usage only for subsonic API
    class PlayQueue : public detail::Object<PlayQueue, PlayQueueId>
    {
    public:
        static PlayQueue create(ISession& session, UserId user, std::string_view name);
        static void destroy(ISession& session, std::span<const PlayQueueId> ids);

        static std::size_t getCount(ISession& session);
        static std::optional<PlayQueue> find(ISession& session, PlayQueueId playQueueId);
        static std::optional<PlayQueue> find(ISession& session, UserId userId, std::string_view name);

        static bool isEmpty(ISession& session, PlayQueueId id);

        static void update(ISession& session, const PlayQueue& playQueue);
        static void clear(ISession& session, PlayQueueId id);
        static void addTrack(ISession& session, PlayQueueId id, TrackId track);

        // Accessors
        std::string_view getName() const { return _name; }
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
        explicit PlayQueue(std::string_view name);

        std::string _name;
        int _currentIndex{};
        std::chrono::duration<int, std::milli> _currentPositionInTrack{};
        Wt::WDateTime _lastModifiedDateTime;
        UserId _userId;
    };
} // namespace lms::db
