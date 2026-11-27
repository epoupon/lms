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
#include "database/objects/RatedReleaseId.hpp"
#include "database/objects/ReleaseId.hpp"
#include "database/objects/Types.hpp"
#include "database/objects/UserId.hpp"
#include "database/objects/detail/Object.hpp"

namespace lms::db
{
    class ISession;
    class User;

    class RatedRelease : public detail::Object<RatedRelease, RatedReleaseId>
    {
    public:
        static RatedRelease create(ISession& session, ReleaseId release, UserId user);
        static void destroy(ISession& session, std::span<const RatedReleaseId> ids);

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
        static std::optional<RatedRelease> find(ISession& session, RatedReleaseId id);
        static std::optional<RatedRelease> find(ISession& session, ReleaseId releaseId, UserId userId);
        static void find(ISession& session, const FindParameters& findParams, const std::function<void(const RatedRelease&)>& func);

        static void update(ISession& session, const RatedRelease& ratedRelease);

        // Accessors
        ReleaseId getReleaseId() const { return _releaseId; }
        UserId getUserId() const { return _userId; }
        Rating getRating() const { return _rating; }
        const Wt::WDateTime& getLastUpdated() const { return _lastUpdated; }

        // Setters
        void setRating(Rating rating) { _rating = rating; }
        void setLastUpdated(const Wt::WDateTime& lastUpdated);

    protected:
        RatedRelease() = default;

        Rating _rating{};
        Wt::WDateTime _lastUpdated; // when it was rated for the last time
        ReleaseId _releaseId;
        UserId _userId;
    };
} // namespace lms::db
