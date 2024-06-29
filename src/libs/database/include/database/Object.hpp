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

#include <Wt/Dbo/ptr.h>
#include <Wt/WSignal.h>

#include "database/IdType.hpp"
#include "database/TransactionChecker.hpp"

namespace lms::db
{
    template<typename T>
    class ObjectPtr
    {
    public:
        ObjectPtr() = default;
        ObjectPtr(const Wt::Dbo::ptr<T>& obj)
            : _obj{ obj } {}
        ObjectPtr(Wt::Dbo::ptr<T>&& obj)
            : _obj{ std::move(obj) } {}

        const T* operator->() const { return _obj.get(); }
        operator bool() const { return _obj.get(); }
        bool operator!() const { return !_obj.get(); }
        bool operator==(const ObjectPtr& other) const { return _obj == other._obj; }
        bool operator!=(const ObjectPtr& other) const { return other._obj != _obj; }

        auto modify()
        {
#if LMS_CHECK_TRANSACTION_ACCESSES
            TransactionChecker::checkWriteTransaction(*_obj.session());
#endif
            return _obj.modify();
        }

        void remove()
        {
#if LMS_CHECK_TRANSACTION_ACCESSES
            TransactionChecker::checkWriteTransaction(*_obj.session());
#endif

            if (_obj->hasOnPreRemove())
                _obj.modify()->onPreRemove();
            _obj.remove();
        }

    private:
        template<typename, typename>
        friend class Object;
        Wt::Dbo::ptr<T> _obj;
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
        template<typename>
        friend class ObjectPtr;

        virtual bool hasOnPreRemove() const { return false; }
        virtual void onPreRemove() {}

        virtual bool hasOnPostCreated() const { return false; }
        virtual void onPostCreated() {}

        // Can get raw dbo ptr only from Objects
        template<typename SomeObject>
        static Wt::Dbo::ptr<SomeObject> getDboPtr(const ObjectPtr<SomeObject>& ptr)
        {
            return ptr._obj;
        }
    };
} // namespace lms::db
