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

#include "logger/Logger.hpp"

#include "ServiceManager.hpp"

namespace Service {

ServiceManager&
ServiceManager::instance()
{
	static ServiceManager instance;
	return instance;
}

ServiceManager::ServiceManager()
{
}

ServiceManager::~ServiceManager()
{
	LMS_LOG(MOD_SERVICE, SEV_NOTICE) << "Stopping services...";
	stop();
}

void
ServiceManager::add(Service::pointer service)
{
	_services.insert(service);
	service->start();
}

void
ServiceManager::del(Service::pointer service)
{
	service->stop();
	_services.erase(service);
}

void
ServiceManager::start(void)
{
	for (Service::pointer service : _services)
		service->start();
}

void
ServiceManager::stop(void)
{
	for (Service::pointer service : _services)
		service->stop();
}


void
ServiceManager::restart(void)
{
	LMS_LOG(MOD_SERVICE, SEV_NOTICE) << "Restarting services...";
	for (Service::pointer service : _services)
		service->restart();
}

} // namespace Service

