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

#include <Wt/Dbo/Field.h>
#include <Wt/WDateTime.h>

#include "database/Object.hpp"
#include "database/Types.hpp"
#include "database/objects/RatedReleaseId.hpp"
#include "database/objects/ReleaseId.hpp"
#include "database/objects/UserId.hpp"

namespace lms::db
{
    class Release;
    class Session;
    class User;

    class RatedRelease final : public Object<RatedRelease, RatedReleaseId>
    {
    public:
        RatedRelease() = default;

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
        static std::size_t getCount(Session& session);
        static pointer find(Session& session, RatedReleaseId id);
        static pointer find(Session& session, ReleaseId releaseId, UserId userId);
        static void find(Session& session, const FindParameters& findParams, std::function<void(const pointer&)> func);

        // Accessors
        ObjectPtr<Release> getRelease() const { return _release; }
        ObjectPtr<User> getUser() const { return _user; }
        Rating getRating() const { return _rating; }
        const Wt::WDateTime& getLastUpdated() const { return _lastUpdated; }

        // Setters
        void setRating(Rating rating) { _rating = rating; }
        void setLastUpdated(const Wt::WDateTime& lastUpdated);

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _rating, "rating");
            Wt::Dbo::field(a, _lastUpdated, "last_updated");

            Wt::Dbo::belongsTo(a, _release, "release", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::belongsTo(a, _user, "user", Wt::Dbo::OnDeleteCascade);
        }

    private:
        friend class Session;
        RatedRelease(ObjectPtr<Release> release, ObjectPtr<User> user);
        static pointer create(Session& session, ObjectPtr<Release> release, ObjectPtr<User> user);

        Rating _rating{};
        Wt::WDateTime _lastUpdated; // when it was rated for the last time

        Wt::Dbo::ptr<Release> _release;
        Wt::Dbo::ptr<User> _user;
    };
} // namespace lms::db
