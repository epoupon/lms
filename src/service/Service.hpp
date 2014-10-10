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

#ifndef SERVICE_HPP
#define SERVICE_HPP

#include <boost/thread.hpp>
#include <memory>
#include <set>

namespace Service {

// Interface class wrapper for running services
class Service
{
	public:

		typedef std::shared_ptr<Service>	pointer;

		Service(const Service&) = delete;
		Service& operator=(const Service&) = delete;

		Service() {}
		virtual ~Service() {}

		virtual void start(void) = 0;
		virtual void stop(void) = 0;
		virtual void restart(void) = 0;

};

} // namespace Service

#endif

