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
#include <optional>

#include "database/IdType.hpp"
#include "database/Object.hpp"
#include "database/Types.hpp"
#include "database/objects/TrackId.hpp"
#include "database/objects/UserId.hpp"

LMS_DECLARE_IDTYPE(TrackBookmarkId)

namespace lms::db
{
    class ISession;
    class Track;
    class User;

    class TrackBookmark : public Object<TrackBookmark, TrackBookmarkId>
    {
    public:
        static pointer create(ISession& session, const ObjectPtr<User>& user, const ObjectPtr<Track>& track);

        // Find utility functions
        static std::size_t getCount(ISession& session);
        static pointer find(ISession& session, TrackBookmarkId id);
        static std::vector<TrackBookmarkId> find(ISession& session, UserId userId, std::optional<Range> range = std::nullopt);
        static pointer find(ISession& session, UserId userId, TrackId trackId);

        // Setters
        void setOffset(std::chrono::milliseconds offset) { _offset = offset; }
        void setComment(std::string_view comment) { _comment = comment; }

        // Getters
        std::chrono::milliseconds getOffset() const { return _offset; }
        std::string_view getComment() const { return _comment; }
        ObjectPtr<Track> getTrack() const;
        ObjectPtr<User> getUser() const;

    protected:
        TrackBookmark() = default;

        static const std::size_t _maxCommentLength = 128;

        std::chrono::duration<int, std::milli> _offset;
        std::string _comment;
    };

} // namespace lms::db
