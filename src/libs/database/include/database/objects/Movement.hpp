/*
 * Copyright (C) 2025 Emeric Poupon
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
#include <string>
#include <string_view>

#include <Wt/Dbo/Field.h>

#include "database/Object.hpp"
#include "database/objects/MovementId.hpp"
#include "database/objects/TrackId.hpp"

namespace lms::db
{
    class Session;
    class Track;

    class Movement final : public Object<Movement, MovementId>
    {
    public:
        static constexpr std::size_t maxNameLength{ 512 };

        Movement() = default;

        static pointer create(Session& session, std::string_view name, std::optional<std::size_t> number, std::optional<std::size_t> count, const ObjectPtr<Track>& track);

        std::string_view getName() const { return _name; }
        std::optional<std::size_t> getNumber() const { return _number; }
        std::optional<std::size_t> getCount() const { return _count; }

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _name, "name");
            Wt::Dbo::field(a, _number, "number");
            Wt::Dbo::field(a, _count, "count");
            Wt::Dbo::belongsTo(a, _track, "track", Wt::Dbo::OnDeleteCascade);
        }

    private:
        friend class Session;
        Movement(std::string_view name, std::optional<std::size_t> number, std::optional<std::size_t> count, const ObjectPtr<Track>& track);

        std::string _name;
        std::optional<int> _number;
        std::optional<int> _count;
        Wt::Dbo::ptr<Track> _track;
    };

} // namespace lms::db
