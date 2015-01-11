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

#include "Types.hpp"

namespace Database {

Video::Video()
{
}

Video::Video(const boost::filesystem::path& p)
: _filePath(p.string())
{
}

Video::pointer
Video::create(Wt::Dbo::Session& session, const boost::filesystem::path& p)
{
	return session.add(new Video(p));
}

Wt::Dbo::collection< Video::pointer >
Video::getAll(Wt::Dbo::Session& session)
{
	return session.find<Video>();
}

Video::pointer
Video::getByPath(Wt::Dbo::Session& session, const boost::filesystem::path& p)
{
	return session.find<Video>().where("path = ?").bind(p.string());
}

} // namespace Video
