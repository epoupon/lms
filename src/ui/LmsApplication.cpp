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

#include <Wt/WEnvironment>
#include <Wt/WBootstrapTheme>
#include <Wt/WVBoxLayout>
#include <Wt/WNavigationBar>
#include <Wt/WStackedWidget>
#include <Wt/WMenu>
#include <Wt/WNavigationBar>
#include <Wt/WPopupMenu>
#include <Wt/WVBoxLayout>
#include <Wt/Auth/Identity>

#include "logger/Logger.hpp"

#include "settings/Settings.hpp"
#include "settings/SettingsFirstConnectionFormView.hpp"

#include "auth/LmsAuth.hpp"
#include "audio/desktop/DesktopAudio.hpp"
#include "audio/mobile/MobileAudio.hpp"
#include "video/VideoWidget.hpp"
#include "common/LineEdit.hpp"

#include "LmsApplication.hpp"

namespace skeletons {
	  extern const char *AuthStrings_xml1;
}

namespace {

bool agentIsMobile()
{
	const Wt::WEnvironment& env = Wt::WApplication::instance()->environment();

	return (env.agentIsIEMobile() || env.agentIsMobileWebKit());
}
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
 _sessionData(dbPath)
{

	Wt::WBootstrapTheme *bootstrapTheme = new Wt::WBootstrapTheme(this);
	bootstrapTheme->setVersion(Wt::WBootstrapTheme::Version3);
	bootstrapTheme->setResponsive(true);
	setTheme(bootstrapTheme);

	useStyleSheet("css/lms.css");

	// Add a resource bundle
	messageResourceBundle().use(appRoot() + "templates");

	setTitle("LMS");

	bool firstConnection;
	{
		Wt::Dbo::Transaction transaction(_sessionData.getDatabaseHandler().getSession());

		firstConnection = (Database::User::getAll(_sessionData.getDatabaseHandler().getSession()).size() == 0);
	}

	// If here is no account in the database, launch the first connection wizard
	if (firstConnection)
		createFirstConnectionUI();
	else
		createLmsUI();

}

void
LmsApplication::createFirstConnectionUI()
{
	// Hack, use the auth widget builtin strings
	builtinLocalizedStrings().useBuiltin(skeletons::AuthStrings_xml1);

	root()->addWidget( new Settings::FirstConnectionFormView(_sessionData));
}

void
LmsApplication::createLmsUI()
{

	_sessionData.getDatabaseHandler().getLogin().changed().connect(this, &LmsApplication::handleAuthEvent);

	LmsAuth *authWidget = new LmsAuth(_sessionData.getDatabaseHandler());

	authWidget->model()->addPasswordAuth(&Database::Handler::getPasswordService());
	authWidget->setRegistrationEnabled(false);

	authWidget->processEnvironment();

	root()->addWidget(authWidget);
}


void
LmsApplication::handleAuthEvent(void)
{
	if (_sessionData.getDatabaseHandler().getLogin().loggedIn())
	{
		Wt::Auth::User user(_sessionData.getDatabaseHandler().getLogin().user());

		LMS_LOG(MOD_UI, SEV_NOTICE) << "User '" << user.identity(Wt::Auth::Identity::LoginName) << "' logged in";

		this->root()->setOverflow(Wt::WContainerWidget::OverflowHidden);

		// Create a Vertical layout: top is the nav bar, bottom is the contents
		Wt::WVBoxLayout *layout = new Wt::WVBoxLayout(this->root());
		// Create a navigation bar with a link to a web page.
		Wt::WNavigationBar *navigation = new Wt::WNavigationBar();
		navigation->setTitle("LMS");
		navigation->setResponsive(true);
		navigation->addStyleClass("main-nav");

		Wt::WStackedWidget *contentsStack = new Wt::WStackedWidget();

		contentsStack->setOverflow(Wt::WContainerWidget::OverflowAuto);

		// Setup a Left-aligned menu.
		Wt::WMenu *leftMenu = new Wt::WMenu(contentsStack);
		navigation->addMenu(leftMenu);

		const Wt::WEnvironment& env = Wt::WApplication::instance()->environment();

		Audio *audio;

		if (agentIsMobile())
			audio = new Desktop::Audio(_sessionData);
		else
			audio = new Mobile::Audio();

		VideoWidget *videoWidget = new VideoWidget(_sessionData);

		leftMenu->addItem("Audio", audio);
		leftMenu->addItem("Video", videoWidget);
		leftMenu->addItem("Settings", new Settings::Settings(_sessionData));

		// Setup a Right-aligned menu.
		Wt::WMenu *rightMenu = new Wt::WMenu();

		navigation->addMenu(rightMenu, Wt::AlignRight);

		Wt::WPopupMenu *popup = new Wt::WPopupMenu();
		popup->addItem("Logout");

		Wt::WMenuItem *item = new Wt::WMenuItem( user.identity(Wt::Auth::Identity::LoginName) );
			item->setMenu(popup);
			rightMenu->addItem(item);

		popup->itemSelected().connect(std::bind([=] (Wt::WMenuItem* item)
		{
			if (item && item->text() == "Logout")
				_sessionData.getDatabaseHandler().getLogin().logout();
		}, std::placeholders::_1));

		// Add a Search control.
		LineEdit *searchEdit = new LineEdit(500);
		searchEdit->setEmptyText("Search...");

		searchEdit->timedChanged().connect(std::bind([=] ()
		{
			// TODO: check which view is activated and search into it
			audio->search(searchEdit->text().toUTF8());
		}));

		navigation->addSearch(searchEdit, Wt::AlignLeft);

		layout->addWidget(navigation);
		layout->addWidget(contentsStack, 1);
		layout->setContentsMargins(0, 0, 0, 0);
	}
	else
	{
		LMS_LOG(MOD_UI, SEV_NOTICE) << "User logged out";

		quit("");
		redirect("/");
	}
}

} // namespace UserInterface


