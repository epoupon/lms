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

#pragma once

#include <vector>

#include <boost/filesystem/path.hpp>

#include <Wt/Dbo/Dbo.h>

namespace Database {

class MediaDirectory
{
	public:
		typedef Wt::Dbo::ptr<MediaDirectory> pointer;

		MediaDirectory() {}
		MediaDirectory(boost::filesystem::path p);

		// Accessors
		static pointer create(Wt::Dbo::Session& session, boost::filesystem::path p);
		static std::vector<MediaDirectory::pointer>	getAll(Wt::Dbo::Session& session);

		static void eraseAll(Wt::Dbo::Session& session);
		static void eraseByPath(Wt::Dbo::Session& session, boost::filesystem::path p);

		boost::filesystem::path	getPath(void) const;

		template<class Action>
			void persist(Action& a)
			{
				Wt::Dbo::field(a, _path,		"path");
			}

	private:

		std::string	_path;
};

} // namespace Database

