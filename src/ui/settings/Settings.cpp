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

#include <Wt/WMenu>
#include <Wt/WStackedWidget>
#include <Wt/WTextArea>
#include <Wt/WHBoxLayout>

#include "SettingsAudioFormView.hpp"
#include "SettingsUserFormView.hpp"
#include "SettingsAccountFormView.hpp"
#include "SettingsDatabaseFormView.hpp"
#include "SettingsMediaDirectories.hpp"
#include "SettingsUsers.hpp"

#include "utils/Logger.hpp"

#include "database/Setting.hpp"
#include "database/updater/DatabaseUpdater.hpp"

#include "LmsApplication.hpp"

#include "Settings.hpp"

namespace UserInterface {
namespace Settings {

using namespace Database;

Settings::Settings(Wt::WContainerWidget* parent)
: Wt::WContainerWidget(parent)
{
	Wt::WHBoxLayout* hLayout = new Wt::WHBoxLayout(this);

	// Create a stack where the contents will be located.
	Wt::WStackedWidget *contents = new Wt::WStackedWidget();

	contents->setStyleClass("contents");
	contents->setOverflow(WContainerWidget::OverflowHidden);

	// TODO menu style on hover
	Wt::WMenu *menu = new Wt::WMenu(contents, Wt::Vertical);
	menu->setStyleClass("nav nav-pills nav-stacked submenu");
	menu->setWidth(150);
	menu->setInternalPathEnabled();
	menu->setInternalBasePath("/settings/");

	hLayout->addWidget(menu);
	hLayout->addWidget(contents, 1);

	std::string userId;
	bool userIsAdmin;
	{
		Wt::Dbo::Transaction transaction(DboSession());

		userId = User::getId(CurrentUser());
		userIsAdmin = CurrentUser()->isAdmin();
	}

	menu->addItem("Audio", new AudioFormView())->setPathComponent("audio");
	if (userIsAdmin)
	{
		MediaDirectories* mediaDirectories = new MediaDirectories();
		mediaDirectories->changed().connect(std::bind([=]
		{
			LMS_LOG(UI, INFO) << "Media directories have changed: requesting imediate scan";

			// On directory change, request an immediate scan
			std::lock_guard<std::mutex> lock(Updater::instance().getMutex());

			Setting::setBool(DboSession(), "manual_scan_requested", true);
			Updater::instance().restart();
		}));
		menu->addItem("Media Folders", mediaDirectories)->setPathComponent("mediadirectories");

		DatabaseFormView* databaseFormView = new DatabaseFormView();
		databaseFormView->changed().connect(std::bind([=]
		{
			std::lock_guard<std::mutex> lock(Updater::instance().getMutex());
			Updater::instance().restart();
		}));
		menu->addItem("Database", databaseFormView)->setPathComponent("database");

		menu->addItem("Users", new Users())->setPathComponent("users");
	}
	else
	{
		menu->addItem("Account", new AccountFormView(userId))->setPathComponent("account");
	}

}

} // namespace Settings
} // namespace UserInterface
