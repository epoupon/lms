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

#ifndef UI_SETTINGS_USERS_HPP
#define UI_SETTINGS_USERS_HPP

#include <Wt/WStackedWidget>
#include <Wt/WContainerWidget>
#include <Wt/WTable>

namespace UserInterface {
namespace Settings {

class Users : public Wt::WContainerWidget
{
	public:
		Users(Wt::WContainerWidget *parent = 0);

		void refresh();

	private:

		void handleUserFormCompleted(bool changed);

		void handleDelUser(Wt::WString loginNameIdentity, std::string id);
		void handleCreateUser(std::string id); // set the id in order to edit the user

		Wt::WStackedWidget*	_stack;
		Wt::WTable*		_table;
};

} // namespace UserInterface
} // namespace Settings

#endif

