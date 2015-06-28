/*
 * Copyright (C) 2015 Emeric Poupon
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

#ifndef API_RESOURCE_HPP
#define API_RESOURCE_HPP

#include <Wt/WResource>

#include <boost/filesystem.hpp>

namespace RestAPI {

class Resource : public Wt::WResource
{
	public:
		Resource(boost::filesystem::path dbPath);

		void handleRequest(const Wt::Http::Request& request,
			     Wt::Http::Response& response);

	private:

		boost::filesystem::path _dbPath;
};

} // namespace API

#endif
