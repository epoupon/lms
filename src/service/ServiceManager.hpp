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

#include <boost/asio.hpp>
#include <set>

#include "Service.hpp"

namespace Service {

// Start/Stop/Reload Services
class ServiceManager
{
	public:

		static ServiceManager& instance();
		~ServiceManager();

		void stopService(Service::pointer service);
		void startService(Service::pointer service);

		void stopAllServices();

		// Return in case of failure/stop by user
		void run();

		template <class T> typename T::pointer getService();

		boost::mutex&	mutex() { return _mutex;}

	private:

		ServiceManager();
		ServiceManager(ServiceManager const&);	// Don't Implement
		void operator=(ServiceManager const&);	// Don't implement

		void restartServices(void);
		void stopServices(void);

		void asyncWaitSignals(void);

		void handleSignal(boost::system::error_code error, int signo);

		boost::mutex	_mutex;

		boost::asio::io_service	_ioService;

		// Listen for interesting signals
		boost::asio::signal_set	_signalSet;

		std::set<Service::pointer>	_services;
};


template <class T> typename T::pointer
ServiceManager::getService()
{
	std::set<Service::pointer>::iterator it;
	for (std::set<Service::pointer>::iterator it = _services.begin(); it != _services.end(); ++it)
	{
		if (typeid(*(*it)) == typeid(T)) {
			return std::dynamic_pointer_cast<T>(*it);
		}
	}
	return std::shared_ptr<T>();
}

} // namespace Service

#endif

