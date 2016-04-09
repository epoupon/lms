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
#include <Wt/WPopupMenu>
#include <Wt/WVBoxLayout>
#include <Wt/Auth/Identity>

#include "config/config.h"
#include "logger/Logger.hpp"
#include "utils/Utils.hpp"

#include "settings/Settings.hpp"
#include "settings/SettingsFirstConnectionFormView.hpp"

#include "auth/LmsAuth.hpp"
#include "audio/desktop/DesktopAudio.hpp"
#include "audio/mobile/MobileAudio.hpp"
#if HAVE_VIDEO
#include "video/VideoWidget.hpp"
#endif
#include "common/LineEdit.hpp"

#include "LmsApplication.hpp"

namespace skeletons {
	  extern const char *AuthStrings_xml1;
}

namespace {

bool agentIsMobile()
{
	const Wt::WEnvironment& env = Wt::WApplication::instance()->environment();
	return (env.agentIsIEMobile()
		|| env.agentIsMobileWebKit()
		|| env.userAgent().find("Mobile") != std::string::npos // Workaround for firefox
		|| env.userAgent().find("Tablet") != std::string::npos // Workaround for firefox
		);
}
}

namespace UserInterface {

Wt::WApplication*
LmsApplication::create(const Wt::WEnvironment& env, Wt::Dbo::SqlConnectionPool& connectionPool)
{
	/*
	 * You could read information from the environment to decide whether
	 * the user has permission to start a new application
	 */
	return new LmsApplication(env, connectionPool);
}

LmsApplication*
LmsApplication::instance()
{
	return reinterpret_cast<LmsApplication*>(Wt::WApplication::instance());
}

/*
 * The env argument contains information about the new session, and
 * the initial request. It must be passed to the Wt::WApplication
 * constructor so it is typically also an argument for your custom
 * application constructor.
*/
LmsApplication::LmsApplication(const Wt::WEnvironment& env, Wt::Dbo::SqlConnectionPool& connectionPool)
: Wt::WApplication(env),
  _db(connectionPool),
  _imageResource(nullptr),
  _transcodeResource(nullptr)
{
	Wt::WBootstrapTheme *bootstrapTheme = new Wt::WBootstrapTheme(this);
	bootstrapTheme->setVersion(Wt::WBootstrapTheme::Version3);
	bootstrapTheme->setResponsive(true);
	setTheme(bootstrapTheme);

	useStyleSheet("css/lms.css");
	useStyleSheet("resources/font-awesome/css/font-awesome.min.css");

	// Add a resource bundle
	messageResourceBundle().use(appRoot() + "templates");

	setTitle("LMS");

	bool firstConnection;
	{
		Wt::Dbo::Transaction transaction(DboSession());

		firstConnection = (Database::User::getAll(DboSession()).size() == 0);
	}

	LMS_LOG(UI, DEBUG) << "Creating root widget. First connection = " << std::boolalpha << firstConnection;

	// If here is no account in the database, launch the first connection wizard
	if (firstConnection)
		createFirstConnectionUI();
	else
		createLmsUI();
}

Database::Handler& DbHandler()
{
	return LmsApplication::instance()->getDbHandler();
}
Wt::Dbo::Session& DboSession()
{
	return DbHandler().getSession();
}

const Wt::Auth::User& CurrentAuthUser()
{
	return DbHandler().getLogin().user();
}

Database::User::pointer CurrentUser()
{
	return DbHandler().getCurrentUser();
}

ImageResource* SessionImageResource()
{
	return LmsApplication::instance()->getImageResource();
}

TranscodeResource* SessionTranscodeResource()
{
	return LmsApplication::instance()->getTranscodeResource();
}

void
LmsApplication::createFirstConnectionUI()
{
	// Hack, use the auth widget builtin strings
	builtinLocalizedStrings().useBuiltin(skeletons::AuthStrings_xml1);

	root()->addWidget( new Settings::FirstConnectionFormView());
}

void
LmsApplication::createLmsUI()
{
	_imageResource = new ImageResource(_db, root());
	_transcodeResource = new TranscodeResource(_db, root());

	DbHandler().getLogin().changed().connect(this, &LmsApplication::handleAuthEvent);

	LmsAuth *authWidget = new LmsAuth();

	authWidget->model()->addPasswordAuth(&Database::Handler::getPasswordService());
	authWidget->setRegistrationEnabled(false);

	authWidget->processEnvironment();

	root()->addWidget(authWidget);
}

void
LmsApplication::handleAuthEvent(void)
{
	if (!DbHandler().getLogin().loggedIn())
	{
		LMS_LOG(UI, INFO) << "User logged out, session = " << Wt::WApplication::instance()->sessionId();

		quit("");
		redirect("/");

		return;
	}

	LMS_LOG(UI, INFO) << "User '" << CurrentAuthUser().identity(Wt::Auth::Identity::LoginName) << "' logged in from '" << Wt::WApplication::instance()->environment().clientAddress() << "', user agent = " << Wt::WApplication::instance()->environment().agent() << ", session = " <<  Wt::WApplication::instance()->sessionId();

	this->root()->setOverflow(Wt::WContainerWidget::OverflowHidden);
	setConfirmCloseMessage("Closing LMS. Are you sure?");

	// Create a Vertical layout: top is the nav bar, bottom is the contents
	Wt::WVBoxLayout *layout = new Wt::WVBoxLayout(this->root());

	Wt::WNavigationBar *navigation = new Wt::WNavigationBar();
	navigation->setTitle("LMS", "https://github.com/epoupon/lms");
	navigation->setResponsive(true);
	navigation->addStyleClass("main-nav");

	Wt::WStackedWidget *contentsStack = new Wt::WStackedWidget();

	contentsStack->setOverflow(Wt::WContainerWidget::OverflowAuto);
	contentsStack->addStyleClass("contents");

	Wt::WMenu *menu = new Wt::WMenu(contentsStack);
	navigation->addMenu(menu, Wt::AlignRight);

	LineEdit *searchEdit = new LineEdit(500);
	navigation->bindWidget("search", searchEdit);
	searchEdit->setEmptyText("Search...");
	searchEdit->addStyleClass("navbar-form navbar-nav");
	searchEdit->setWidth(150);
	// TODO add a span with a search icon

	menu->setInternalPathEnabled();
	menu->setInternalBasePath("/");

	Audio *audio;
	if (agentIsMobile())
		audio = new Mobile::Audio();
	else
		audio = new Desktop::Audio();

	menu->addItem("Audio", audio)->setPathComponent("audio");;
#if defined HAVE_VIDEO
	menu->addItem("Video", new VideoWidget())->setPathComponent("video");
#endif
	menu->addItem("Settings", new Settings::Settings())->setPathComponent("settings");

	Wt::WPopupMenu *popup = new Wt::WPopupMenu();
	popup->addItem("Logout");

	Wt::WMenuItem *item = new Wt::WMenuItem( CurrentAuthUser().identity(Wt::Auth::Identity::LoginName) );
	item->setMenu(popup);
	menu->addItem(item);

	popup->itemSelected().connect(std::bind([=] (Wt::WMenuItem* item)
	{
		if (item && item->text() == "Logout")
			DbHandler().getLogin().logout();
	}, std::placeholders::_1));

	searchEdit->timedChanged().connect(std::bind([=] ()
	{
		// TODO: check which view is activated and search into it
		audio->search(searchEdit->text().toUTF8());
	}));

	layout->addWidget(navigation);
	layout->addWidget(contentsStack, 1);
	layout->setContentsMargins(0, 0, 0, 0);

	if (agentIsMobile())
		setInternalPath("/audio/search/preview", true);
	else
		setInternalPath("/audio");

}

} // namespace UserInterface


