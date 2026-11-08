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

#include "database/objects/Mood.hpp"

#include "ObjectPtrImpl.hpp"

namespace lms::db::dbo
{
    class Track;

    class Mood final : public db::Mood, public Wt::Dbo::Dbo<Mood>
    {
    public:
        Mood() = default;
        using db::Mood::Mood;

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _name, "name");
            Wt::Dbo::hasMany(a, _tracks, Wt::Dbo::ManyToMany, "track_mood", "", Wt::Dbo::OnDeleteCascade);
        }

        Wt::Dbo::collection<Wt::Dbo::ptr<Track>> _tracks;
    };
} // namespace lms::db::dbo

namespace lms::db
{
    template<>
    struct DboOf<Mood>
    {
        using type = dbo::Mood;
    };
} // namespace lms::db
