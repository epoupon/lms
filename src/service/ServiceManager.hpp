/*
 * Copyright (C) 2013 Emeric Poupon
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

#ifndef SERVICE_CONTROLER_HPP
#define SERVICE_CONTROLER_HPP

#include <set>

#include "Service.hpp"

namespace Service {

// Start/Stop/Reload Services
class ServiceManager
{
	public:

		static ServiceManager& instance();
		~ServiceManager();

		void add(Service::pointer service);
		void del(Service::pointer service);
		void clear();

		void start();
		void stop();
		void restart();

		template <class T> typename T::pointer get();

		boost::mutex&	mutex() { return _mutex;}

	private:

		ServiceManager();
		ServiceManager(ServiceManager const&);	// Don't Implement
		void operator=(ServiceManager const&);	// Don't implement

		boost::mutex	_mutex;

		std::set<Service::pointer>	_services;
};


template <class T> typename T::pointer
ServiceManager::get()
{
	std::set<Service::pointer>::iterator it;
	for (Service::pointer service : _services)
	{
		if (typeid(*(service)) == typeid(T)) {
			return std::dynamic_pointer_cast<T>(service);
		}
	}
	return std::shared_ptr<T>();
}

} // namespace Service

#endif

