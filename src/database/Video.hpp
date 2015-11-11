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

#ifndef _VIDEO_TYPES_HPP
#define _VIDEO_TYPES_HPP

#include <string>

#include <boost/filesystem.hpp>

#include <Wt/Dbo/Dbo>
#include <Wt/Dbo/WtSqlTraits>

#include <Wt/WDateTime>

namespace Database {

class Video
{
	public:

		typedef Wt::Dbo::ptr<Video> pointer;

		Video();
		Video( const boost::filesystem::path& p);

		// Find utilities
		static pointer getByPath(Wt::Dbo::Session& session, const boost::filesystem::path& p);
		static Wt::Dbo::collection< pointer > getAll(Wt::Dbo::Session& session);
		static std::vector<boost::filesystem::path> getAllPaths(Wt::Dbo::Session& session);

		// Create utility
		static pointer	create(Wt::Dbo::Session& session, const boost::filesystem::path& p);

		// Modifiers
		void setName(const std::string& name)				{ _name = name; }
		void setDuration(boost::posix_time::time_duration duration)	{ _duration = duration; }
		void setLastWriteTime(boost::posix_time::ptime time)		{ _fileLastWrite = time; }

		// Accessors
		std::string 				getName(void) const	{ return _name; }
		boost::posix_time::time_duration	getDuration(void) const	{ return _duration; }
		boost::filesystem::path			getPath(void) const	{ return _filePath; }
		boost::posix_time::ptime		getLastWriteTime(void) const { return _fileLastWrite; }

		template<class Action>
			void persist(Action& a)
			{
				Wt::Dbo::field(a, _name,		"name");
				Wt::Dbo::field(a, _duration,		"duration");
				Wt::Dbo::field(a, _fileLastWrite,	"last_write");
				Wt::Dbo::field(a, _filePath,		"path");
			}

	private:

		std::string				_name;
		boost::posix_time::time_duration	_duration;
		std::string				_filePath;
		boost::posix_time::ptime		_fileLastWrite;

}; // Video


} // namespace Database

#endif

