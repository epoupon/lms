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

#include "UsersView.hpp"

#include <Wt/WPushButton.h>
#include <Wt/WMessageBox.h>
#include <Wt/WTemplate.h>

#include "database/User.hpp"
#include "utils/Logger.hpp"

#include "LmsApplication.hpp"

namespace UserInterface {

UsersView::UsersView()
 : Wt::WTemplate(Wt::WString::tr("Lms.Admin.Users.template"))
{
	addFunction("tr", &Wt::WTemplate::Functions::tr);

	_container = bindNew<Wt::WContainerWidget>("users");

	Wt::WPushButton* addBtn = bindNew<Wt::WPushButton>("add-btn", Wt::WString::tr("Lms.Admin.Users.add"));
	addBtn->clicked().connect(std::bind([=]
	{
		LmsApp->setInternalPath("/admin/user", true);
	}));

	wApp->internalPathChanged().connect(std::bind([=]
	{
		refreshView();
	}));

	refreshView();
}

void
UsersView::refreshView()
{
	if (!wApp->internalPathMatches("/admin/users"))
		return;

	_container->clear();

	Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

	auto users = Database::User::getAll(LmsApp->getDboSession());
	for (auto user : users)
	{
		auto userId = std::to_string(user.id());
		Wt::WTemplate* entry = _container->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Admin.Users.template.entry"));

		Wt::Auth::User authUser = LmsApp->getDb().getUserDatabase().findWithId(userId);
		if (!authUser.isValid()) {
			LMS_LOG(UI, ERROR) << "Skipping invalid userId = " << user.id();
			continue;
		}

		auto login = authUser.identity(Wt::Auth::Identity::LoginName);
		entry->bindString("name", login, Wt::TextFormat::Plain);

		// Create tag
		if (user->isAdmin() || user->isDemo())
		{
			entry->setCondition("if-tag", true);
			entry->bindString("tag", Wt::WString::tr(user->isAdmin() ? "Lms.Admin.Users.admin" : "Lms.Admin.Users.demo"));
		}

		// Don't edit ourself this way
		if (LmsApp->getUser() == user)
			continue;

		entry->setCondition("if-edit", true);
		Wt::WPushButton* editBtn = entry->bindNew<Wt::WPushButton>("edit-btn", Wt::WString::tr("Lms.Admin.Users.edit"));
		editBtn->clicked().connect(std::bind([=]
		{
			LmsApp->setInternalPath("/admin/user/" + std::to_string(user.id()), true);
		}));

		Wt::WPushButton* delBtn = entry->bindNew<Wt::WPushButton>("del-btn", Wt::WString::tr("Lms.Admin.Users.del"));
		delBtn->clicked().connect([=]
		{
			auto msgBox = delBtn->addChild(std::make_unique<Wt::WMessageBox>(Wt::WString::tr("Lms.Admin.Users.del-user"),
				Wt::WString::tr("Lms.Admin.Users.del-user-name"),
				Wt::Icon::Warning, Wt::StandardButton::Yes | Wt::StandardButton::No));

			msgBox->setModal(true);
			msgBox->buttonClicked().connect([=] (Wt::StandardButton btn)
			{
				if (btn == Wt::StandardButton::Yes)
				{
					Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

					auto authUser = LmsApp->getDb().getUserDatabase().findWithId(userId);
					auto user = LmsApp->getDb().getUser(authUser);
					LmsApp->getDb().getUserDatabase().deleteUser( authUser );
					user.remove();
					_container->removeWidget(entry);
				}
				else
					delBtn->removeChild(msgBox);
			});

			msgBox->show();
		});
	}
}

} // namespace UserInterface


