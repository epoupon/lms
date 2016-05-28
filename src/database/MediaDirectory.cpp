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

#include "utils/Utils.hpp"

#include "Types.hpp"

namespace Database {

MediaDirectory::MediaDirectory(boost::filesystem::path p, Type type)
: _type(type),
 _path(stringTrimEnd(p.string(), "/\\"))
{
}

MediaDirectory::pointer
MediaDirectory::create(Wt::Dbo::Session& session, boost::filesystem::path p, Type type)
{
	return session.add( new MediaDirectory( p, type ) );
}

void
MediaDirectory::eraseAll(Wt::Dbo::Session& session)
{
	for (auto dir : getAll(session))
		dir.remove();
}

std::vector<MediaDirectory::pointer>
MediaDirectory::getAll(Wt::Dbo::Session& session)
{
	Wt::Dbo::collection< MediaDirectory::pointer > res = session.find<MediaDirectory>();

	return std::vector<MediaDirectory::pointer>(res.begin(), res.end());
}

std::vector<MediaDirectory::pointer>
MediaDirectory::getByType(Wt::Dbo::Session& session, Type type)
{
	Wt::Dbo::collection< MediaDirectory::pointer > res = session.find<MediaDirectory>().where("type = ?").bind (type);

	return std::vector<MediaDirectory::pointer>(res.begin(), res.end());
}

MediaDirectory::pointer
MediaDirectory::get(Wt::Dbo::Session& session, boost::filesystem::path p, Type type)
{
	return session.find<MediaDirectory>().where("path = ?").where("type = ?").bind( p.string()).bind(type);
}

boost::filesystem::path
MediaDirectory::getPath(void) const
{
	return boost::filesystem::path(stringTrimEnd(_path, "/\\"));
}

} // namespace Database
