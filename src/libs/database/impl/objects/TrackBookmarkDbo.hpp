/*
 * Copyright (C) 2026 Emeric Poupon
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

#include <Wt/Dbo/Dbo.h>
#include <Wt/Dbo/Field.h>

#include "database/objects/TrackBookmark.hpp"

#include "DboAccess.hpp"

namespace lms::db::dbo
{
    class Track;
    class User;

    class TrackBookmark final : public db::TrackBookmark, public Wt::Dbo::Dbo<TrackBookmark>
    {
    public:
        TrackBookmark() = default;
        using db::TrackBookmark::TrackBookmark;

        TrackBookmark(Wt::Dbo::ptr<User> user, Wt::Dbo::ptr<Track> track);

        void syncIds()
        {
            _id = id();
            _trackId = _track.id();
            _userId = _user.id();
        }

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _offset, "offset");
            Wt::Dbo::field(a, _comment, "comment");
            Wt::Dbo::belongsTo(a, _track, "track", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::belongsTo(a, _user, "user", Wt::Dbo::OnDeleteCascade);
        }

        Wt::Dbo::ptr<User> _user;
        Wt::Dbo::ptr<Track> _track;
    };
} // namespace lms::db::dbo

namespace lms::db
{
    template<>
    struct DboOf<TrackBookmark>
    {
        using type = dbo::TrackBookmark;
    };
} // namespace lms::db
