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

#include <optional>
#include <span>

#include <Wt/WDateTime.h>

#include "database/Types.hpp"
#include "database/objects/ArtistFeedbackId.hpp"
#include "database/objects/ArtistId.hpp"
#include "database/objects/Types.hpp"
#include "database/objects/UserId.hpp"
#include "database/objects/detail/Object.hpp"

namespace lms::db
{
    class ISession;
    class User;

    class ArtistFeedback : public detail::Object<ArtistFeedback, ArtistFeedbackId>
    {
    public:
        static ArtistFeedback create(ISession& session, ArtistId artist, UserId user);
        static void destroy(ISession& session, std::span<const ArtistFeedbackId> ids);

        // Search utility
        static std::size_t getCount(ISession& session);
        static std::optional<ArtistFeedback> find(ISession& session, ArtistFeedbackId id);
        static std::optional<ArtistFeedback> find(ISession& session, ArtistId artistId, UserId userId);

        static void update(ISession& session, const ArtistFeedback& artistFeedback);

        // Accessors
        ArtistId getArtistId() const { return _artistId; }
        UserId getUserId() const { return _userId; }
        const Wt::WDateTime& getDateTime() const { return _dateTime; }
        FeedbackValue getValue() const { return _value; }

        // Setters
        void setDateTime(const Wt::WDateTime& dateTime);
        void setValue(FeedbackValue value) { _value = value; }

    protected:
        ArtistFeedback() = default;

        Wt::WDateTime _dateTime; // when the value was last changed to non-None
        FeedbackValue _value{ FeedbackValue::None };
        ArtistId _artistId;
        UserId _userId;
    };
} // namespace lms::db
