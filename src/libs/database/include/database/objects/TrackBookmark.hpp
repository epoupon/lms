/*
 * Copyright (C) 2020 Emeric Poupon
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

#include "database/IdType.hpp"
#include "database/objects/TrackId.hpp"
#include "database/objects/UserId.hpp"
#include "database/objects/detail/Object.hpp"

LMS_DECLARE_IDTYPE(TrackBookmarkId)

namespace lms::db
{
    class ISession;
    class User;

    class TrackBookmark : public detail::Object<TrackBookmark, TrackBookmarkId>
    {
    public:
        static TrackBookmark create(ISession& session, UserId user, TrackId track);
        static void destroy(ISession& session, std::span<const TrackBookmarkId> ids);

        // Find utility functions
        static std::size_t getCount(ISession& session);
        static std::optional<TrackBookmark> find(ISession& session, TrackBookmarkId id);
        static void find(ISession& session, UserId userId, const std::function<void(const TrackBookmark&)>& func);
        static std::optional<TrackBookmark> find(ISession& session, UserId userId, TrackId trackId);

        static void update(ISession& session, const TrackBookmark& trackBookmark);

        // Setters
        void setOffset(std::chrono::milliseconds offset) { _offset = offset; }
        void setComment(std::string_view comment) { _comment = comment; }

        // Getters
        std::chrono::milliseconds getOffset() const { return _offset; }
        std::string_view getComment() const { return _comment; }
        TrackId getTrackId() const { return _trackId; }
        UserId getUserId() const { return _userId; }

    protected:
        TrackBookmark() = default;

        static const std::size_t _maxCommentLength = 128;

        std::chrono::duration<int, std::milli> _offset;
        std::string _comment;
        TrackId _trackId;
        UserId _userId;
    };

} // namespace lms::db
