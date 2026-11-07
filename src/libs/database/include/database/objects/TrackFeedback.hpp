/*
 * Copyright (C) 2021 Emeric Poupon
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
#include <Wt/WDateTime.h>

#include "database/Object.hpp"
#include "database/Types.hpp"
#include "database/objects/TrackFeedbackId.hpp"
#include "database/objects/TrackId.hpp"
#include "database/objects/Types.hpp"
#include "database/objects/UserId.hpp"

namespace lms::db
{
    class ISession;
    class Track;
    class User;

    class TrackFeedback final : public Object<TrackFeedback, TrackFeedbackId>
    {
    public:
        TrackFeedback() = default;

        static pointer create(ISession& session, ObjectPtr<Track> track, ObjectPtr<User> user);

        struct FindParameters
        {
            UserId user;                     // only feedbacks belonging to this user
            TrackFeedbackId lastRetrievedId; // if valid, only feedbacks with an id greater than this one (requires sortMethod == Id)
            std::optional<Range> range;
            TrackFeedbackSortMethod sortMethod{ TrackFeedbackSortMethod::None };

            FindParameters& setUser(UserId _user)
            {
                user = _user;
                return *this;
            }
            FindParameters& setLastRetrievedId(TrackFeedbackId _lastRetrievedId)
            {
                lastRetrievedId = _lastRetrievedId;
                return *this;
            }
            FindParameters& setRange(std::optional<Range> _range)
            {
                range = _range;
                return *this;
            }
            FindParameters& setSortMethod(TrackFeedbackSortMethod _sortMethod)
            {
                sortMethod = _sortMethod;
                return *this;
            }
        };

        // Search utility
        static std::size_t getCount(ISession& session);
        static pointer find(ISession& session, TrackFeedbackId id);
        static pointer find(ISession& session, TrackId trackId, UserId userId);
        static void find(ISession& session, const FindParameters& params, const std::function<void(const pointer&)>& func);

        // Accessors
        ObjectPtr<Track> getTrack() const { return _track; }
        ObjectPtr<User> getUser() const { return _user; }
        const Wt::WDateTime& getDateTime() const { return _dateTime; }
        FeedbackValue getValue() const { return _value; }

        // Setters
        void setDateTime(const Wt::WDateTime& dateTime);
        void setValue(FeedbackValue value) { _value = value; }

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _dateTime, "date_time");
            Wt::Dbo::field(a, _value, "value");

            Wt::Dbo::belongsTo(a, _track, "track", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::belongsTo(a, _user, "user", Wt::Dbo::OnDeleteCascade);
        }

    private:
        TrackFeedback(ObjectPtr<Track> track, ObjectPtr<User> user);

        Wt::WDateTime _dateTime; // when the value was last changed to non-None
        FeedbackValue _value{ FeedbackValue::None };

        Wt::Dbo::ptr<Track> _track;
        Wt::Dbo::ptr<User> _user;
    };
} // namespace lms::db
