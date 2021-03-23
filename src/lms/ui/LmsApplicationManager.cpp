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

#include "LmsApplicationManager.hpp"

#include "LmsApplication.hpp"

namespace UserInterface
{
	void
	LmsApplicationManager::registerApplication(LmsApplication& application)
	{
		{
			std::scoped_lock lock {_mutex};
			m_applications[application.getUserId()].insert(&application);
		}

		applicationRegistered.emit(application);
	}

	void
	LmsApplicationManager::unregisterApplication(LmsApplication& application)
	{
		{
			std::scoped_lock lock {_mutex};
			m_applications[application.getUserId()].erase(&application);
		}

		applicationUnregistered.emit(application);
	}

} // UserInterface
