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

#include <type_traits>

#include "database/IdType.hpp"

namespace lms::db::detail
{
    // Base of the entities, handled as values: changes are saved by an explicit update
    // The database mapping of each entity lives in a private subclass, see impl/objects/*Dbo.hpp
    template<typename T, typename ObjectIdType>
    class Object
    {
        static_assert(std::is_base_of_v<db::IdType, ObjectIdType>);
        static_assert(!std::is_same_v<db::IdType, ObjectIdType>);

    public:
        using IdType = ObjectIdType;

        IdType getId() const { return _id; }

    protected:
        Object() = default;

        IdType _id;
    };
} // namespace lms::db::detail
