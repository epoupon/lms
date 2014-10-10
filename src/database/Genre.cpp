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

#include "AudioTypes.hpp"

namespace Database {

Genre::Genre()
{
}

Genre::Genre(const std::string& name)
: _name( std::string(name, 0, _maxNameLength) )
{
}


Genre::pointer
Genre::getByName(Wt::Dbo::Session& session, const std::string& name)
{
	// TODO use like search
	return session.find<Genre>().where("name = ?").bind( std::string(name, 0, _maxNameLength) );
}

Genre::pointer
Genre::getNone(Wt::Dbo::Session& session)
{
	pointer res = getByName(session, "<None>");
	if (!res)
		res = create(session, "<None>");
	return res;
}

bool
Genre::isNone(void) const
{
	return (_name == "<None>");
}

Genre::pointer
Genre::create(Wt::Dbo::Session& session, const std::string& name)
{
	return session.add(new Genre(name));
}

Wt::Dbo::collection<Genre::pointer>
Genre::getAll(Wt::Dbo::Session& session, std::size_t offset, std::size_t size)
{
	return session.find<Genre>().offset(offset).limit(size);
}

} // namespace Database
