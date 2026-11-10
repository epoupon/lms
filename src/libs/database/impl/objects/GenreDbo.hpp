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

#include "database/objects/Genre.hpp"

#include "DboAccess.hpp"

namespace lms::db::dbo
{
    class Track;

    class Genre final : public db::Genre, public Wt::Dbo::Dbo<Genre>
    {
    public:
        Genre() = default;
        using db::Genre::Genre;

        void syncIds() { _id = id(); }

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _name, "name");
            Wt::Dbo::field(a, _trackCount, "track_count");
            Wt::Dbo::field(a, _releaseCount, "release_count");
            Wt::Dbo::hasMany(a, _tracks, Wt::Dbo::ManyToMany, "track_genre", "", Wt::Dbo::OnDeleteCascade);
        }

        Wt::Dbo::collection<Wt::Dbo::ptr<Track>> _tracks;
    };
} // namespace lms::db::dbo

namespace lms::db
{
    template<>
    struct DboOf<Genre>
    {
        using type = dbo::Genre;
    };
} // namespace lms::db
