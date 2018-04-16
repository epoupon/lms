/*
 * Copyright (C) 2016 Emeric Poupon
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

#include "Setting.hpp"

namespace Database {

bool
Setting::exists(Wt::Dbo::Session& session, std::string setting)
{
	Wt::Dbo::Transaction transaction(session);
	return (getByName(session, setting) != Setting::pointer());
}

std::string
Setting::getString(Wt::Dbo::Session& session, std::string setting, std::string defaultValue)
{
	Wt::Dbo::Transaction transaction(session);

	pointer res = getByName(session, setting);
	if (!res)
		return defaultValue;

	return res->_value;
}

bool
Setting::getBool(Wt::Dbo::Session& session, std::string setting, bool defaultValue)
{
	Wt::Dbo::Transaction transaction(session);

	pointer res = getByName(session, setting);
	if (!res)
		return defaultValue;

	return (res->_value == "true");
}

Wt::WTime
Setting::getTime(Wt::Dbo::Session& session, std::string setting, Wt::WTime defaultValue)
{
	Wt::Dbo::Transaction transaction(session);

	pointer res = getByName(session, setting);
	if (!res)
		return defaultValue;

	return Wt::WTime::fromString(res->_value);
}

int
Setting::getInt(Wt::Dbo::Session& session, std::string setting, int defaultValue)
{
	Wt::Dbo::Transaction transaction(session);

	pointer res = getByName(session, setting);
	if (!res)
		return defaultValue;

	return std::stoi(res->_value);
}



Setting::pointer
Setting::create(Wt::Dbo::Session& session, std::string name)
{
	return session.add<Setting>(std::make_unique<Setting>(name));
}

Setting::pointer
Setting::getByName(Wt::Dbo::Session& session, std::string name)
{
	return session.find<Setting>().where("name = ?").bind(name);
}

Setting::pointer
Setting::getOrCreateByName(Wt::Dbo::Session& session, std::string name)
{
	pointer res = getByName(session, name);
	if (!res)
		res = create(session, name);

	return res;
}

void
Setting::setString(Wt::Dbo::Session& session, std::string setting, std::string value)
{
	Wt::Dbo::Transaction transaction(session);
	getOrCreateByName(session, setting).modify()->_value = value;
}

void
Setting::setBool(Wt::Dbo::Session& session, std::string setting, bool value)
{
	Wt::Dbo::Transaction transaction(session);
	getOrCreateByName(session, setting).modify()->_value = (value ? "true" : "false");
}

void
Setting::setTime(Wt::Dbo::Session& session, std::string setting, Wt::WTime value)
{
	Wt::Dbo::Transaction transaction(session);
	getOrCreateByName(session, setting).modify()->_value = value.toString().toUTF8();
}

void
Setting::setInt(Wt::Dbo::Session& session, std::string setting, int value)
{
	Wt::Dbo::Transaction transaction(session);
	getOrCreateByName(session, setting).modify()->_value = std::to_string(value);
}


} // namespace Database

