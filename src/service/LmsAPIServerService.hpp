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

#ifndef REMOTE_SERVER_SERVICE_HPP
#define REMOTE_SERVER_SERVICE_HPP

#include <Wt/Dbo/SqlConnectionPool>

#include "config/config.h"

#include "Service.hpp"

#include "lms-api/server/Server.hpp"

namespace Service {

class LmsAPIService : public Service
{
	public:

		LmsAPIService(Wt::Dbo::SqlConnectionPool& connectionPool);

		void start(void);
		void stop(void);
		void restart(void);

	private:

		LmsAPI::Server::Server	_server;
};

} // namespace Service

#endif
