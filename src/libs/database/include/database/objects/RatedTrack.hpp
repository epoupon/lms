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

#include <optional>
#include <span>

#include <Wt/WDateTime.h>

#include "database/Types.hpp"
#include "database/objects/RatedTrackId.hpp"
#include "database/objects/TrackId.hpp"
#include "database/objects/Types.hpp"
#include "database/objects/UserId.hpp"
#include "database/objects/detail/Object.hpp"

namespace lms::db
{
    class ISession;
    class User;

    class RatedTrack : public detail::Object<RatedTrack, RatedTrackId>
    {
    public:
        static RatedTrack create(ISession& session, TrackId track, UserId user);
        static void destroy(ISession& session, std::span<const RatedTrackId> ids);

        struct FindParameters
        {
            UserId user; // and this user
            std::optional<Range> range;

            FindParameters& setUser(UserId _user)
            {
                user = _user;
                return *this;
            }
            FindParameters& setRange(std::optional<Range> _range)
            {
                range = _range;
                return *this;
            }
        };

        // Search utility
        static std::size_t getCount(ISession& session);
        static std::optional<RatedTrack> find(ISession& session, RatedTrackId id);
        static std::optional<RatedTrack> find(ISession& session, TrackId trackId, UserId userId);
        static void find(ISession& session, const FindParameters& findParams, const std::function<void(const RatedTrack&)>& func);

        static void update(ISession& session, const RatedTrack& ratedTrack);

        // Accessors
        TrackId getTrackId() const { return _trackId; }
        UserId getUserId() const { return _userId; }
        Rating getRating() const { return _rating; }
        const Wt::WDateTime& getLastUpdated() const { return _lastUpdated; }

        // Setters
        void setRating(Rating rating) { _rating = rating; }
        void setLastUpdated(const Wt::WDateTime& lastUpdated);

    protected:
        RatedTrack() = default;

        Rating _rating{};
        Wt::WDateTime _lastUpdated; // when it was rated for the last time
        TrackId _trackId;
        UserId _userId;
    };
} // namespace lms::db
