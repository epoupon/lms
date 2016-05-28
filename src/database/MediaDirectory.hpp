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

#ifndef DATABASE_MEDIA_DIRECTORY_HPP
#define DATABASE_MEDIA_DIRECTORY_HPP

#include <vector>

#include <boost/filesystem/path.hpp>

#include <Wt/Dbo/Dbo>
#include <Wt/Dbo/WtSqlTraits>
namespace Database {

class MediaDirectory
{
	public:

		typedef Wt::Dbo::ptr<MediaDirectory> pointer;

		enum Type {
			Audio = 1,
			Video = 2,
		};

		MediaDirectory() {}
		MediaDirectory(boost::filesystem::path p, Type type);

		// Accessors
		static pointer create(Wt::Dbo::Session& session, boost::filesystem::path p, Type type);
		static std::vector<MediaDirectory::pointer>	getAll(Wt::Dbo::Session& session);
		static std::vector<MediaDirectory::pointer>	getByType(Wt::Dbo::Session& session, Type type);
		static pointer get(Wt::Dbo::Session& session, boost::filesystem::path p, Type type);

		static void eraseAll(Wt::Dbo::Session& session);

		Type			getType(void) const	{ return _type; }
		boost::filesystem::path	getPath(void) const;

		template<class Action>
			void persist(Action& a)
			{
				Wt::Dbo::field(a, _type,		"type");
				Wt::Dbo::field(a, _path,		"path");
			}

	private:

		Type		_type;
		std::string	_path;

};

} // namespace Database

#endif

