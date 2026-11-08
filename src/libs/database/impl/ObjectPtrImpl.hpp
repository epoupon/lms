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
#include <type_traits>
#include <utility>

#include <Wt/Dbo/Impl.h>

#include "database/Object.hpp"

#include "Session.hpp"

#define LMS_DB_INSTANTIATE_OBJECT(C)                                                             \
    DBO_INSTANTIATE_TEMPLATES(lms::db::DboOf<C>::type)                                           \
    template class lms::db::ObjectPtr<C>;                                                        \
    template lms::db::ObjectPtr<C>::ObjectPtr(const Wt::Dbo::ptr<lms::db::DboOf<C>::type>&);     \
    template lms::db::ObjectPtr<C>::ObjectPtr(Wt::Dbo::ptr<lms::db::DboOf<C>::type>&&) noexcept; \
    template class lms::db::Object<C, C::IdType>;

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

    template<typename T>
    auto& ObjectPtr<T>::dboPtr()
    {
        return *std::launder(reinterpret_cast<Wt::Dbo::ptr<typename DboOf<T>::type>*>(_storage));
    }

    template<typename T>
    const auto& ObjectPtr<T>::dboPtr() const
    {
        return *std::launder(reinterpret_cast<const Wt::Dbo::ptr<typename DboOf<T>::type>*>(_storage));
    }

    template<typename T>
    ObjectPtr<T>::Mutator::Mutator(const ObjectPtr& ptr)
        : _ptr{ ptr }
        , _obj{ ptr.dboPtr().modify().operator->() }
    {
    }

    template<typename T>
    ObjectPtr<T>::Mutator::~Mutator()
    {
        // A setter may have flushed the object: it has to be marked as dirty again
        _ptr.dboPtr().modify();
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
    template<typename DboType>
        requires std::derived_from<DboType, T>
    ObjectPtr<T>::ObjectPtr(const Wt::Dbo::ptr<DboType>& obj)
    {
        static_assert(std::is_same_v<DboType, typename DboOf<T>::type>);
        new (_storage) Wt::Dbo::ptr<DboType>{ obj };
    }

    template<typename T>
    template<typename DboType>
        requires std::derived_from<DboType, T>
    ObjectPtr<T>::ObjectPtr(Wt::Dbo::ptr<DboType>&& obj) noexcept
    {
        static_assert(std::is_same_v<DboType, typename DboOf<T>::type>);
        new (_storage) Wt::Dbo::ptr<DboType>{ std::move(obj) };
    }

    template<typename T>
    ObjectPtr<T>::ObjectPtr(const ObjectPtr& other)
    {
        new (_storage) Wt::Dbo::ptr<typename DboOf<T>::type>{ other.dboPtr() };
    }

    template<typename T>
    ObjectPtr<T>::ObjectPtr(ObjectPtr&& other) noexcept
    {
        new (_storage) Wt::Dbo::ptr<typename DboOf<T>::type>{ std::move(other.dboPtr()) };
    }

    template<typename T>
    ObjectPtr<T>& ObjectPtr<T>::operator=(const ObjectPtr& other)
    {
        dboPtr() = other.dboPtr();
        return *this;
    }

    template<typename T>
    ObjectPtr<T>& ObjectPtr<T>::operator=(ObjectPtr&& other) noexcept
    {
        dboPtr() = std::move(other.dboPtr());
        return *this;
    }

    template<typename T>
    ObjectPtr<T>::~ObjectPtr()
    {
        std::destroy_at(&dboPtr());
    }

    template<typename T>
    const T* ObjectPtr<T>::operator->() const
    {
        return dboPtr().get();
    }

    template<typename T>
    ObjectPtr<T>::operator bool() const
    {
        return dboPtr().get();
    }

    template<typename T>
    bool ObjectPtr<T>::operator!() const
    {
        return !dboPtr().get();
    }

    template<typename T>
    bool ObjectPtr<T>::operator==(const ObjectPtr& other) const
    {
        return dboPtr() == other.dboPtr();
    }

    template<typename T>
    bool ObjectPtr<T>::operator!=(const ObjectPtr& other) const
    {
        return other.dboPtr() != dboPtr();
    }

    template<typename T>
    typename ObjectPtr<T>::Mutator ObjectPtr<T>::modify()
    {
        checkWriteTransaction(*dboPtr().session());
        return Mutator{ *this };
    }

    template<typename T>
    void ObjectPtr<T>::remove()
    {
        checkWriteTransaction(*dboPtr().session());
        dboPtr().remove();
    }

    template<typename T, typename ObjectIdType>
    ObjectIdType Object<T, ObjectIdType>::getId() const
    {
        using Dbo = typename DboOf<T>::type;
        return static_cast<const Dbo*>(this)->Wt::Dbo::template Dbo<Dbo>::id();
    }
} // namespace lms::db
