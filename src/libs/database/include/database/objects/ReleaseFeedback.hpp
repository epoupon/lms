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

#include <Wt/WDateTime.h>

#include "database/Object.hpp"
#include "database/Types.hpp"
#include "database/objects/ReleaseFeedbackId.hpp"
#include "database/objects/ReleaseId.hpp"
#include "database/objects/Types.hpp"
#include "database/objects/UserId.hpp"

namespace lms::db
{
    class ISession;
    class Release;
    class User;

    class ReleaseFeedback : public Object<ReleaseFeedback, ReleaseFeedbackId>
    {
    public:
        static pointer create(ISession& session, const ObjectPtr<Release>& release, const ObjectPtr<User>& user);

        // Search utility
        static std::size_t getCount(ISession& session);
        static pointer find(ISession& session, ReleaseFeedbackId id);
        static pointer find(ISession& session, ReleaseId releaseId, UserId userId);

        // Accessors
        ObjectPtr<Release> getRelease() const;
        ObjectPtr<User> getUser() const;
        const Wt::WDateTime& getDateTime() const { return _dateTime; }
        FeedbackValue getValue() const { return _value; }

        // Setters
        void setDateTime(const Wt::WDateTime& dateTime);
        void setValue(FeedbackValue value) { _value = value; }

    protected:
        ReleaseFeedback() = default;

        Wt::WDateTime _dateTime; // when the value was last changed to non-None
        FeedbackValue _value{ FeedbackValue::None };
    };
} // namespace lms::db
