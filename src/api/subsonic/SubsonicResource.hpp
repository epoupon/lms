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
#pragma once

#include <boost/optional.hpp>

#include <Wt/WResource.h>
#include <Wt/Http/Response.h>

#include "database/DatabaseHandler.hpp"

namespace API::Subsonic
{

class SubsonicResource final : public Wt::WResource
{
	public:
		SubsonicResource(Wt::Dbo::SqlConnectionPool& connectionPool);

		static std::vector<std::string> getPaths();
	private:


		void handleRequest(const Wt::Http::Request &request, Wt::Http::Response &response) override;

		Database::Handler	_db;
};

} // namespace
