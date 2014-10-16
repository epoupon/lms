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

#include <Wt/WBootstrapTheme>
#include <Wt/WVBoxLayout>
#include <Wt/WHBoxLayout>
#include <Wt/WNavigationBar>
#include <Wt/WStackedWidget>
#include <Wt/WMenu>
#include <Wt/WNavigationBar>
#include <Wt/WPopupMenu>
#include <Wt/WPopupMenuItem>
#include <Wt/WVBoxLayout>
#include <Wt/Auth/Identity>

#include "settings/Settings.hpp"


#include "auth/LmsAuth.hpp"

#include "logger/Logger.hpp"

#include "LmsHome.hpp"
#include "settings/SettingsFirstConnectionFormView.hpp"

#include "LmsApplication.hpp"

namespace skeletons {
	  extern const char *AuthStrings_xml1;
}


namespace UserInterface {

Wt::WApplication*
LmsApplication::create(const Wt::WEnvironment& env, boost::filesystem::path dbPath)
{
	/*
	 * You could read information from the environment to decide whether
	 * the user has permission to start a new application
	 */
	return new LmsApplication(env, dbPath);
}

/*
 * The env argument contains information about the new session, and
 * the initial request. It must be passed to the Wt::WApplication
 * constructor so it is typically also an argument for your custom
 * application constructor.
*/
LmsApplication::LmsApplication(const Wt::WEnvironment& env, boost::filesystem::path dbPath)
: Wt::WApplication(env),
 _sessionData(dbPath),
 _home(nullptr)
{

	Wt::WBootstrapTheme *bootstrapTheme = new Wt::WBootstrapTheme(this);
	bootstrapTheme->setVersion(Wt::WBootstrapTheme::Version3);
	bootstrapTheme->setResponsive(true);
	setTheme(bootstrapTheme);

	// Add a resource bundle
	messageResourceBundle().use(appRoot() + "templates");

	setTitle("LMS");                               // application title

	// Create a Vertical layout: top is the nav bar, bottom is the contents
	Wt::WVBoxLayout *layout = new Wt::WVBoxLayout(this->root());
	// Create a navigation bar with a link to a web page.
	Wt::WNavigationBar *navigation = new Wt::WNavigationBar();
	navigation->setTitle("LMS");
	navigation->setResponsive(true);
	navigation->addStyleClass("main-nav");

	Wt::WStackedWidget *contentsStack = new Wt::WStackedWidget();

	// Setup a Left-aligned menu.
	Wt::WMenu *leftMenu = new Wt::WMenu(contentsStack);
	navigation->addMenu(leftMenu);

	_audioWidget = new AudioWidget(_sessionData);
	_videoWidget = new VideoWidget(_sessionData);

	leftMenu->addItem("Audio", _audioWidget);
	leftMenu->addItem("Video", _videoWidget);
//	leftMenu->addItem("Settings", new Settings::Settings(_sessionData));

	// Setup a Right-aligned menu.
	Wt::WMenu *rightMenu = new Wt::WMenu();

	navigation->addMenu(rightMenu, Wt::AlignRight);

	Wt::WPopupMenu *popup = new Wt::WPopupMenu();
	popup->addItem("Logout");

//	popup->itemSelected().connect(this, &LmsHome::handleUserMenuSelected);

/*	Wt::WMenuItem *item = new Wt::WMenuItem( user.identity(Wt::Auth::Identity::LoginName) );
	item->setMenu(popup);
	rightMenu->addItem(item);*/

	// Add a Search control.
/*	_searchEdit = new Wt::WLineEdit();
	_searchEdit->setEmptyText("Search...");

	_searchEdit->enterPressed().connect(this, &LmsHome::handleSearch);
*/
//	navigation->addSearch(_searchEdit, Wt::AlignLeft);

	layout->addWidget(navigation);
	layout->addWidget(contentsStack, 1);
	layout->setContentsMargins(0, 0, 0, 0);
}

} // namespace UserInterface


