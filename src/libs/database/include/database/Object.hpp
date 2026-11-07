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

#include <cstddef>

#include <Wt/Dbo/ptr.h>

#include "database/IdType.hpp"

namespace lms::db
{
    // Members are defined in impl/ObjectPtrImpl.hpp and instantiated next to each entity
    template<typename T>
    class ObjectPtr
    {
    public:
        class Mutator
        {
        public:
            Mutator(const Mutator&) = delete;
            Mutator& operator=(const Mutator&) = delete;
            ~Mutator();

            T* operator->() const { return _obj; }

        private:
            friend class ObjectPtr;
            explicit Mutator(const ObjectPtr& ptr);

            const ObjectPtr& _ptr;
            T* _obj;
        };

        ObjectPtr() noexcept;
        ObjectPtr(const Wt::Dbo::ptr<T>& obj);
        ObjectPtr(Wt::Dbo::ptr<T>&& obj) noexcept;
        ObjectPtr(const ObjectPtr& other);
        ObjectPtr(ObjectPtr&& other) noexcept;
        ObjectPtr& operator=(const ObjectPtr& other);
        ObjectPtr& operator=(ObjectPtr&& other) noexcept;
        ~ObjectPtr();

        const T* operator->() const;
        operator bool() const;
        bool operator!() const;
        bool operator==(const ObjectPtr& other) const;
        bool operator!=(const ObjectPtr& other) const;

        Mutator modify();
        void remove();

    private:
        template<typename, typename>
        friend class Object;

        Wt::Dbo::ptr<T>& dboPtr();
        const Wt::Dbo::ptr<T>& dboPtr() const;

        // Holds a Wt::Dbo::ptr: an opaque buffer so that this class won't depend on the mapped class
        alignas(void*) std::byte _storage[2 * sizeof(void*)];
    };

    template<typename T, typename ObjectIdType>
    class Object : public Wt::Dbo::Dbo<T>
    {
        static_assert(std::is_base_of_v<db::IdType, ObjectIdType>);
        static_assert(!std::is_same_v<db::IdType, ObjectIdType>);

    public:
        using pointer = ObjectPtr<T>;
        using IdType = ObjectIdType;

        IdType getId() const { return Wt::Dbo::Dbo<T>::self()->Wt::Dbo::template Dbo<T>::id(); }

        // catch some misuses
        typename Wt::Dbo::dbo_traits<T>::IdType id() const = delete;

    protected:
        // Can get raw dbo ptr only from Objects
        template<typename SomeObject>
        static const Wt::Dbo::ptr<SomeObject>& getDboPtr(const ObjectPtr<SomeObject>& ptr)
        {
            return ptr.dboPtr();
        }
    };
} // namespace lms::db
