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

#include "database/objects/Movement.hpp"

#include "ObjectPtrImpl.hpp"

namespace lms::db::dbo
{
    class Track;

    class Movement final : public db::Movement, public Wt::Dbo::Dbo<Movement>
    {
    public:
        Movement() = default;
        using db::Movement::Movement;

        Movement(std::string_view name, std::optional<std::size_t> number, std::optional<std::size_t> count, const ObjectPtr<db::Track>& track);

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _name, "name");
            Wt::Dbo::field(a, _number, "number");
            Wt::Dbo::field(a, _count, "count");
            Wt::Dbo::belongsTo(a, _track, "track", Wt::Dbo::OnDeleteCascade);
        }

        Wt::Dbo::ptr<Track> _track;
    };
} // namespace lms::db::dbo

namespace lms::db
{
    template<>
    struct DboOf<Movement>
    {
        using type = dbo::Movement;
    };
} // namespace lms::db
