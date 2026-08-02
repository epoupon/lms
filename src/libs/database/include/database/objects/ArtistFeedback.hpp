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

#include <Wt/Dbo/Field.h>
#include <Wt/WDateTime.h>

#include "database/Object.hpp"
#include "database/Types.hpp"
#include "database/objects/ArtistFeedbackId.hpp"
#include "database/objects/ArtistId.hpp"
#include "database/objects/Types.hpp"
#include "database/objects/UserId.hpp"

namespace lms::db
{
    class Artist;
    class Session;
    class User;

    class ArtistFeedback final : public Object<ArtistFeedback, ArtistFeedbackId>
    {
    public:
        ArtistFeedback() = default;

        // Search utility
        static std::size_t getCount(Session& session);
        static pointer find(Session& session, ArtistFeedbackId id);
        static pointer find(Session& session, ArtistId artistId, UserId userId);

        // Accessors
        ObjectPtr<Artist> getArtist() const { return _artist; }
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

            Wt::Dbo::belongsTo(a, _artist, "artist", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::belongsTo(a, _user, "user", Wt::Dbo::OnDeleteCascade);
        }

    private:
        friend class Session;
        ArtistFeedback(ObjectPtr<Artist> artist, ObjectPtr<User> user);
        static pointer create(Session& session, ObjectPtr<Artist> artist, ObjectPtr<User> user);

        Wt::WDateTime _dateTime; // when the value was last changed to non-None
        FeedbackValue _value{ FeedbackValue::None };

        Wt::Dbo::ptr<Artist> _artist;
        Wt::Dbo::ptr<User> _user;
    };
} // namespace lms::db
