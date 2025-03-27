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

#include <string>

namespace lms::db
{
    class IdType
    {
    public:
        using ValueType = long long;

        IdType();
        IdType(ValueType id);

        bool isValid() const;
        std::string toString() const;

        ValueType getValue() const { return _id; }
        auto operator<=>(const IdType& other) const = default;

    private:
        ValueType _id;
    };

#define LMS_DECLARE_IDTYPE(name)                                             \
    namespace lms::db                                                        \
    {                                                                        \
        class name : public IdType                                           \
        {                                                                    \
        public:                                                              \
            using IdType::IdType;                                            \
            auto operator<=>(const name& other) const = default;             \
        };                                                                   \
    }                                                                        \
    namespace std                                                            \
    {                                                                        \
        template<>                                                           \
        class hash<lms::db::name>                                            \
        {                                                                    \
        public:                                                              \
            size_t operator()(lms::db::name id) const                        \
            {                                                                \
                return std::hash<lms::db::name::ValueType>()(id.getValue()); \
            }                                                                \
        };                                                                   \
    } // ns std
} // namespace lms::db
