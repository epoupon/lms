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
#include "services/database/IdType.hpp"

namespace Database
{
	template <typename T>
	class ObjectPtr
	{
		public:
			ObjectPtr() = default;
			ObjectPtr(Wt::Dbo::ptr<T> obj) : _obj {obj} {}

			const T* operator->() const { return _obj.get(); }
			operator bool() const { return _obj.get(); }
			bool operator!() const { return !_obj.get(); }

			auto modify() { return _obj.modify(); }
			void remove() { _obj.remove(); }

		private:
			template <typename, typename> friend class Object;
			Wt::Dbo::ptr<T> _obj;
	};

	template <typename T, typename ObjectIdType>
	class Object : public Wt::Dbo::Dbo<T>
	{
		static_assert(std::is_base_of_v<Database::IdType, ObjectIdType>);
		static_assert(!std::is_same_v<Database::IdType, ObjectIdType>);

		public:
			using pointer = ObjectPtr<T>;
			using IdType = ObjectIdType;

			IdType getId() const { return Wt::Dbo::Dbo<T>::self()->Wt::Dbo::template Dbo<T>::id(); }

			// catch some misuses
			typename Wt::Dbo::dbo_traits<T>::IdType id() const = delete;

		protected:
			// Can get raw dbo ptr only from Objects
			template <typename SomeObject>
			static
			Wt::Dbo::ptr<SomeObject> getDboPtr(ObjectPtr<SomeObject> ptr) { return ptr._obj; }
	};
}
