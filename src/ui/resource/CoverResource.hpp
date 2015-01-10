/*
 * Copyright (C) 2014 Emeric Poupon
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

#ifndef COVER_RESOURCE_HPP_
#define COVER_RESOURCE_HPP_

#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>

#include <Wt/WResource>

#include "database/DatabaseHandler.hpp"

namespace UserInterface {

class CoverResource : public Wt::WResource
{
	public:
		CoverResource(Database::Handler& db,
				std::size_t size,		// size * size pixels
				Wt::WObject *parent = 0);
		~CoverResource();

		void handleRequest(const Wt::Http::Request& request, Wt::Http::Response& response);

	private:

		Database::Handler&		_db;
		std::size_t			_size;
};

} // namespace UserInterface

#endif
