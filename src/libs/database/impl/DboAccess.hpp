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

#include <new>
#include <type_traits>
#include <utility>

#include <Wt/Dbo/ptr.h>

#include "database/Object.hpp"

namespace lms::db
{
    // Mapped class of an entity: specialized next to each private mapped subclass
    template<typename T>
    struct DboOf;

    template<typename T>
        requires(!std::is_const_v<T>)
    typename DboOf<T>::type& getDbo(T& obj)
    {
        return static_cast<typename DboOf<T>::type&>(obj);
    }

    template<typename T>
    const typename DboOf<T>::type& getDbo(const T& obj)
    {
        return static_cast<const typename DboOf<T>::type&>(obj);
    }

    class ObjectPtrAccess
    {
    public:
        template<typename T>
        static auto& dboPtr(ObjectPtr<T>& ptr)
        {
            return *std::launder(reinterpret_cast<Wt::Dbo::ptr<typename DboOf<T>::type>*>(ptr._storage));
        }

        template<typename T>
        static const auto& dboPtr(const ObjectPtr<T>& ptr)
        {
            return *std::launder(reinterpret_cast<const Wt::Dbo::ptr<typename DboOf<T>::type>*>(ptr._storage));
        }
    };

    template<typename T>
    const auto& getDboPtr(const ObjectPtr<T>& ptr)
    {
        return ObjectPtrAccess::dboPtr(ptr);
    }

    template<typename T>
    inline constexpr bool isObjectPtr{ false };

    template<typename T>
    inline constexpr bool isObjectPtr<ObjectPtr<T>>{ true };

    template<typename DboType>
    concept MappedEntity = isObjectPtr<typename DboType::pointer>;

    template<MappedEntity DboType>
    typename DboType::pointer toObjectPtr(Wt::Dbo::ptr<DboType> ptr)
    {
        typename DboType::pointer res;
        ObjectPtrAccess::dboPtr(res) = std::move(ptr);
        return res;
    }
} // namespace lms::db
