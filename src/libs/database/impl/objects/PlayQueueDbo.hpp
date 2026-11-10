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
#include <Wt/Dbo/collection.h>

#include "database/objects/PlayQueue.hpp"

#include "DboAccess.hpp"

namespace lms::db::dbo
{
    class Track;
    class User;

    class PlayQueue final : public db::PlayQueue, public Wt::Dbo::Dbo<PlayQueue>
    {
    public:
        PlayQueue() = default;
        using db::PlayQueue::PlayQueue;

        PlayQueue(Wt::Dbo::ptr<User> user, std::string_view name);

        void syncIds()
        {
            _id = id();
            _userId = _user.id();
        }

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _name, "name");
            Wt::Dbo::field(a, _currentIndex, "current_index");
            Wt::Dbo::field(a, _currentPositionInTrack, "current_position_in_track");
            Wt::Dbo::field(a, _lastModifiedDateTime, "last_modified_date_time");

            Wt::Dbo::belongsTo(a, _user, "user", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::hasMany(a, _tracks, Wt::Dbo::ManyToMany, "playqueue_track", "", Wt::Dbo::OnDeleteCascade);
        }

        Wt::Dbo::ptr<User> _user;
        Wt::Dbo::collection<Wt::Dbo::ptr<Track>> _tracks;
    };
} // namespace lms::db::dbo

namespace lms::db
{
    template<>
    struct DboOf<PlayQueue>
    {
        using type = dbo::PlayQueue;
    };
} // namespace lms::db
