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

#include "logger/Logger.hpp"

#include "service/ServiceManager.hpp"
#include "service/DatabaseUpdateService.hpp"

#include "Settings.hpp"

namespace UserInterface {
namespace Settings {

Settings::Settings(SessionData& sessionData, Wt::WContainerWidget* parent)
: Wt::WContainerWidget(parent),
_sessionData(sessionData)
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

	hLayout->addWidget(menu);
	hLayout->addWidget(contents, 1);

	std::string userId;
	bool userIsAdmin;
	{
		Wt::Dbo::Transaction transaction( sessionData.getDatabaseHandler().getSession());
		::Database::User::pointer user = sessionData.getDatabaseHandler().getCurrentUser();
		userId = Database::User::getId(user);
		userIsAdmin = user->isAdmin();
	}

	menu->addItem("Audio", new AudioFormView(sessionData));
	if (userIsAdmin)
	{
		MediaDirectories* mediaDirectory = new MediaDirectories(sessionData);
		mediaDirectory->changed().connect(this, &Settings::handleDatabaseDirectoriesChanged);
		menu->addItem("Media Folders", mediaDirectory);

		DatabaseFormView* databaseFormView = new DatabaseFormView(sessionData);
		databaseFormView->changed().connect(this, &Settings::restartDatabaseUpdateService);
		menu->addItem("Database Update", databaseFormView);

		menu->addItem("Users", new Users(sessionData));
	}
	else
	{
		menu->addItem("Account", new AccountFormView(sessionData, userId));
	}

}

void
Settings::handleDatabaseDirectoriesChanged()
{
	LMS_LOG(MOD_UI, SEV_NOTICE) << "Media directories have changed: requesting imediate scan";
	// On directory add or delete, request an immediate scan
	{
		Wt::Dbo::Transaction transaction(_sessionData.getDatabaseHandler().getSession());
		Database::MediaDirectorySettings::get(_sessionData.getDatabaseHandler().getSession()).modify()->setManualScanRequested(true);
	}

	restartDatabaseUpdateService();
}

void
Settings::restartDatabaseUpdateService()
{
	// Restarting the update service
	boost::lock_guard<boost::mutex> serviceLock (Service::ServiceManager::instance().mutex());

	Service::DatabaseUpdateService::pointer service = Service::ServiceManager::instance().getService<Service::DatabaseUpdateService>();
	if (service)
		service->restart();
}

} // namespace Settings
} // namespace UserInterface
