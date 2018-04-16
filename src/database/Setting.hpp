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

#pragma once

#include <Wt/Dbo/Dbo.h>
#include <Wt/WTime.h>

namespace Database {

// class meant to store general settings
class Setting
{
	public:
		using pointer = Wt::Dbo::ptr<Setting>;

		Setting() {}
		Setting(std::string name) : _name(name) {}

		// check if a setting exists or not
		static bool exists(Wt::Dbo::Session& session, std::string setting);

		// Getters
		// Nested transactions
		static std::string getString(Wt::Dbo::Session& session, std::string setting, std::string defaultValue = "");
		static bool getBool(Wt::Dbo::Session& session, std::string setting, bool defaultValue = false);
		static Wt::WTime getTime(Wt::Dbo::Session& session, std::string setting, Wt::WTime defaultValue = Wt::WTime());
		static int getInt(Wt::Dbo::Session& session, std::string setting, int defaultValue = 0);

		// Setters
		// Nested transactions
		static void setString(Wt::Dbo::Session& session, std::string setting, std::string value);
		static void setBool(Wt::Dbo::Session& session, std::string setting, bool value);
		static void setTime(Wt::Dbo::Session& session, std::string setting, Wt::WTime value);
		static void setInt(Wt::Dbo::Session& session, std::string setting, int value);

		template<class Action>
		void persist(Action& a)
		{
			Wt::Dbo::field(a,	_name,		"name");
			Wt::Dbo::field(a,	_value,		"value");
		}

	private:
		static pointer getByName(Wt::Dbo::Session& session, std::string name);
		static pointer create(Wt::Dbo::Session& session, std::string name);
		static pointer getOrCreateByName(Wt::Dbo::Session& session, std::string name);

		std::string	_name;
		std::string	_value;
};


} // namespace Database

