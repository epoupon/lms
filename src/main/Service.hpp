/*
 * Copyright (C) 2019 Emeric Poupon
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

#include <memory>

template <typename T>
class ServiceProvider
{
	public:

		template <class ...Args>
		static
		T&
		create(Args&&... args)
		{
			assign(std::make_unique<T>(std::forward<Args>(args)...));
			return *get();
		}
		static void assign(std::unique_ptr<T> service) { _service = std::move(service); }
		static void clear() { _service.reset(); }

		static T* get() { return _service.get(); }

	private:
		static std::unique_ptr<T> _service;
};

template <typename T>
std::unique_ptr<T> ServiceProvider<T>::_service = {};

template <typename T>
T*
getService()
{
	return ServiceProvider<T>::get();
}


