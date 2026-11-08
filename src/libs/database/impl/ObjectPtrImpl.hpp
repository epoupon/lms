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

#include <memory>
#include <new>
#include <utility>

#include <Wt/Dbo/Impl.h>

#include "database/Object.hpp"

#include "DboAccess.hpp"
#include "Session.hpp"

#define LMS_DB_INSTANTIATE_OBJECT(C)                   \
    DBO_INSTANTIATE_TEMPLATES(lms::db::DboOf<C>::type) \
    template class lms::db::ObjectPtr<C>;              \
    template class lms::db::Object<C, C::IdType>;

namespace lms::db
{
    template<typename T>
    ObjectPtr<T>::Mutator::Mutator(const ObjectPtr& ptr)
        : _ptr{ ptr }
        , _obj{ getDboPtr(ptr).modify().operator->() }
    {
    }

    template<typename T>
    ObjectPtr<T>::Mutator::~Mutator()
    {
        // A setter may have flushed the object: it has to be marked as dirty again
        getDboPtr(_ptr).modify();
    }

    template<typename T>
    ObjectPtr<T>::ObjectPtr() noexcept
    {
        using DboPtr = Wt::Dbo::ptr<typename DboOf<T>::type>;
        static_assert(sizeof(DboPtr) == sizeof(_storage));
        static_assert(alignof(DboPtr) == alignof(void*));

        new (_storage) DboPtr{};
    }

    template<typename T>
    ObjectPtr<T>::ObjectPtr(const ObjectPtr& other)
    {
        new (_storage) Wt::Dbo::ptr<typename DboOf<T>::type>{ getDboPtr(other) };
    }

    template<typename T>
    ObjectPtr<T>::ObjectPtr(ObjectPtr&& other) noexcept
    {
        new (_storage) Wt::Dbo::ptr<typename DboOf<T>::type>{ std::move(ObjectPtrAccess::dboPtr(other)) };
    }

    template<typename T>
    ObjectPtr<T>& ObjectPtr<T>::operator=(const ObjectPtr& other)
    {
        ObjectPtrAccess::dboPtr(*this) = getDboPtr(other);
        return *this;
    }

    template<typename T>
    ObjectPtr<T>& ObjectPtr<T>::operator=(ObjectPtr&& other) noexcept
    {
        ObjectPtrAccess::dboPtr(*this) = std::move(ObjectPtrAccess::dboPtr(other));
        return *this;
    }

    template<typename T>
    ObjectPtr<T>::~ObjectPtr()
    {
        std::destroy_at(&ObjectPtrAccess::dboPtr(*this));
    }

    template<typename T>
    const T* ObjectPtr<T>::operator->() const
    {
        return getDboPtr(*this).get();
    }

    template<typename T>
    ObjectPtr<T>::operator bool() const
    {
        return getDboPtr(*this).get();
    }

    template<typename T>
    bool ObjectPtr<T>::operator!() const
    {
        return !getDboPtr(*this).get();
    }

    template<typename T>
    bool ObjectPtr<T>::operator==(const ObjectPtr& other) const
    {
        return getDboPtr(*this) == getDboPtr(other);
    }

    template<typename T>
    bool ObjectPtr<T>::operator!=(const ObjectPtr& other) const
    {
        return getDboPtr(other) != getDboPtr(*this);
    }

    template<typename T>
    typename ObjectPtr<T>::Mutator ObjectPtr<T>::modify()
    {
        checkWriteTransaction(*getDboPtr(*this).session());
        return Mutator{ *this };
    }

    template<typename T>
    void ObjectPtr<T>::remove()
    {
        checkWriteTransaction(*getDboPtr(*this).session());
        ObjectPtrAccess::dboPtr(*this).remove();
    }

    template<typename T, typename ObjectIdType>
    ObjectIdType Object<T, ObjectIdType>::getId() const
    {
        using Dbo = typename DboOf<T>::type;
        return static_cast<const Dbo*>(this)->Wt::Dbo::template Dbo<Dbo>::id();
    }
} // namespace lms::db
