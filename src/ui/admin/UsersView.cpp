/*
 * Copyright (C) 2018 Emeric Poupon
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

#include <Wt/WTemplate>
#include <Wt/WPushButton>

#include "database/Types.hpp"
#include "utils/Logger.hpp"

#include "LmsApplication.hpp"
#include "UsersView.hpp"

namespace UserInterface {

UsersView::UsersView(Wt::WContainerWidget *parent)
 : Wt::WContainerWidget(parent)
{
	LMS_LOG(UI, DEBUG) << "TEST";
	auto t = new Wt::WTemplate(Wt::WString::tr("Lms.Admin.Users.template"), this);
	t->addFunction("tr", &Wt::WTemplate::Functions::tr);

	_container = new Wt::WContainerWidget();
	t->bindWidget("users", _container);

	auto addBtn = new Wt::WPushButton(Wt::WString::tr("Lms.Admin.Users.add"));
	t->bindWidget("add-btn", addBtn);

	refresh();
}

void
UsersView::refresh()
{
	_container->clear();

	Wt::Dbo::Transaction transaction(DboSession());

	auto users = Database::User::getAll(DboSession());
	for (auto user : users)
	{
		auto entry = new Wt::WTemplate(Wt::WString::tr("Lms.Admin.Users.template.entry"), _container);

		Wt::Auth::User authUser = DbHandler().getUserDatabase().findWithId(std::to_string(user.id()));

		if (!authUser.isValid()) {
			LMS_LOG(UI, ERROR) << "Skipping invalid userId = " << user.id();
			continue;
		}

		entry->bindString("name", authUser.identity(Wt::Auth::Identity::LoginName), Wt::PlainText);

		// Don't edit ourself this way
		if (CurrentUser() != user)
		{
			entry->setCondition("if-edit", true);
			auto editBtn = new Wt::WPushButton(Wt::WString::tr("Lms.Admin.Users.edit"));
			entry->bindWidget("edit-btn", editBtn);

			auto delBtn = new Wt::WPushButton(Wt::WString::tr("Lms.Admin.Users.del"));
			entry->bindWidget("del-btn", delBtn);
		}
	}
}

} // namespace UserInterface


