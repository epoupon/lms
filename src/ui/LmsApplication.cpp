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
#include <Wt/WNavigationBar>
#include <Wt/WStackedWidget>
#include <Wt/WMenu>
#include <Wt/WPopupMenu>
#include <Wt/WText>

#include "config/config.h"
#include "utils/Logger.hpp"
#include "utils/Utils.hpp"

#include "auth/LmsAuth.hpp"
#include "Explore.hpp"
#include "HomeView.hpp"
#include "MediaPlayer.hpp"
#include "PlayQueueView.hpp"

#include "settings/DatabaseView.hpp"
#include "settings/FirstConnectionView.hpp"

#include "LmsApplication.hpp"

namespace skeletons {
	  extern const char *AuthStrings_xml1;
}

namespace UserInterface {

Wt::WApplication*
LmsApplication::create(const Wt::WEnvironment& env, Wt::Dbo::SqlConnectionPool& connectionPool, Scanner::MediaScanner& scanner)
{
	/*
	 * You could read information from the environment to decide whether
	 * the user has permission to start a new application
	 */
	return new LmsApplication(env, connectionPool, scanner);
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
LmsApplication::LmsApplication(const Wt::WEnvironment& env, Wt::Dbo::SqlConnectionPool& connectionPool, Scanner::MediaScanner& scanner)
: Wt::WApplication(env),
  _db(connectionPool),
  _scanner(scanner),
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
	messageResourceBundle().use(appRoot() + "artist");
	messageResourceBundle().use(appRoot() + "artists");
	messageResourceBundle().use(appRoot() + "mediaplayer");
	messageResourceBundle().use(appRoot() + "messages");
	messageResourceBundle().use(appRoot() + "playqueue");
	messageResourceBundle().use(appRoot() + "release");
	messageResourceBundle().use(appRoot() + "releases");
	messageResourceBundle().use(appRoot() + "settings-database");
	messageResourceBundle().use(appRoot() + "settings-first-connection");
	messageResourceBundle().use(appRoot() + "tracks");
	messageResourceBundle().use(appRoot() + "templates");

	setTitle("LMS");

	// If here is no account in the database, launch the first connection wizard
	bool firstConnection;
	{
		Wt::Dbo::Transaction transaction(DboSession());

		firstConnection = (Database::User::getAll(DboSession()).size() == 0);
	}

	LMS_LOG(UI, DEBUG) << "Creating root widget. First connection = " << std::boolalpha << firstConnection;

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
	return Database::User::getById(DboSession(), 1);
//	return DbHandler().getCurrentUser();
}

ImageResource* SessionImageResource()
{
	return LmsApplication::instance()->getImageResource();
}

TranscodeResource* SessionTranscodeResource()
{
	return LmsApplication::instance()->getTranscodeResource();
}

Scanner::MediaScanner& MediaScanner()
{
	return LmsApplication::instance()->getMediaScanner();
}

void
LmsApplication::goHome()
{
	setInternalPath("/home", true);
}

void
LmsApplication::createFirstConnectionUI()
{
	// Hack, use the auth widget builtin strings
	builtinLocalizedStrings().useBuiltin(skeletons::AuthStrings_xml1);

	root()->addWidget(new Settings::FirstConnectionView());
}

void
LmsApplication::createLmsUI()
{
	_imageResource = new ImageResource(_db, root());
	_transcodeResource = new TranscodeResource(_db, root());

	handleAuthEvent();
	/*
	DbHandler().getLogin().changed().connect(this, &LmsApplication::handleAuthEvent);

	LmsAuth *authWidget = new LmsAuth();

	authWidget->model()->addPasswordAuth(&Database::Handler::getPasswordService());
	authWidget->setRegistrationEnabled(false);

	authWidget->processEnvironment();

	root()->addWidget(authWidget);
	*/
}


enum IdxRoot
{
	IdxHome		= 0,
	IdxExplore,
	IdxPlayQueue,
	IdxSettingsDatabase,
};



static void
handlePathChange(Wt::WStackedWidget* stack)
{
	static const std::map<std::string, int> indexes =
	{
		{ "/home",		IdxHome },
		{ "/artists",		IdxExplore },
		{ "/artist",		IdxExplore },
		{ "/releases",		IdxExplore },
		{ "/release",		IdxExplore },
		{ "/tracks",		IdxExplore },
		{ "/playqueue",		IdxPlayQueue },
		{ "/settings/database",	IdxSettingsDatabase },
	};

	LMS_LOG(UI, DEBUG) << "Internal path changed to '" << wApp->internalPath() << "'";

	for (auto index : indexes)
	{
		if (wApp->internalPathMatches(index.first))
		{
			stack->setCurrentIndex(index.second);
			return;
		}
	}

	wApp->setInternalPath("/home", true);
}

void
LmsApplication::handleAuthEvent(void)
{
	/*
	if (!DbHandler().getLogin().loggedIn())
	{
		LMS_LOG(UI, INFO) << "User logged out, session = " << Wt::WApplication::instance()->sessionId();

		quit("");
		redirect("/");

		return;
	}

	LMS_LOG(UI, INFO) << "User '" << CurrentAuthUser().identity(Wt::Auth::Identity::LoginName) << "' logged in from '" << Wt::WApplication::instance()->environment().clientAddress() << "', user agent = " << Wt::WApplication::instance()->environment().agent() << ", session = " <<  Wt::WApplication::instance()->sessionId();
*/
	setConfirmCloseMessage(Wt::WString::tr("msg-quit-confirm"));

	auto main = new Wt::WTemplate(Wt::WString::tr("template-main"), root());

	// Navbar
	auto navbar = new Wt::WNavigationBar();
	navbar->setTitle("LMS", Wt::WLink(Wt::WLink::InternalPath, "/home"));
	navbar->setResponsive(true);

	main->bindWidget("navbar-top", navbar);

	auto menu = new Wt::WMenu();
	{
		auto menuItem = menu->insertItem(0, Wt::WString::tr("msg-artists"));
		menuItem->setLink(Wt::WLink(Wt::WLink::InternalPath, "/artists"));
		menuItem->setSelectable(false);
	}
	{
		auto menuItem = menu->insertItem(1, Wt::WString::tr("msg-releases"));
		menuItem->setLink(Wt::WLink(Wt::WLink::InternalPath, "/releases"));
		menuItem->setSelectable(false);
	}
	{
		auto menuItem = menu->insertItem(2, Wt::WString::tr("msg-tracks"));
		menuItem->setLink(Wt::WLink(Wt::WLink::InternalPath, "/tracks"));
		menuItem->setSelectable(false);
	}
	{
		auto menuItem = menu->insertItem(3, Wt::WString::tr("msg-playqueue"));
		menuItem->setLink(Wt::WLink(Wt::WLink::InternalPath, "/playqueue"));
		menuItem->setSelectable(false);
	}
	{
		auto menuItem = menu->insertItem(4, Wt::WString::tr("msg-settings"));
		menuItem->setSelectable(false);

		Wt::WPopupMenu *settings = new Wt::WPopupMenu();
		auto dbSettings = settings->insertItem(0, Wt::WString::tr("msg-settings-database"));
		dbSettings->setLink(Wt::WLink(Wt::WLink::InternalPath, "/settings/database"));
		dbSettings->setSelectable(false);

		menuItem->setMenu(settings);
	}

	navbar->addMenu(menu);

	// Contents
	Wt::WStackedWidget* mainStack = new Wt::WStackedWidget();
	main->bindWidget("contents", mainStack);

	mainStack->addWidget(new Home());

	auto explore = new Explore();
	mainStack->addWidget(explore);

	auto playqueue = new PlayQueue();
	mainStack->addWidget(playqueue);

	auto databaseSettings = new Settings::DatabaseView();
	mainStack->addWidget(databaseSettings);

	explore->tracksAdd.connect(std::bind([=] (std::vector<Database::Track::pointer> tracks)
	{
		playqueue->addTracks(tracks);
	}, std::placeholders::_1));

	explore->tracksPlay.connect(std::bind([=] (std::vector<Database::Track::pointer> tracks)
	{
		playqueue->playTracks(tracks);
	}, std::placeholders::_1));

	// MediaPlayer
	auto player = new MediaPlayer();
	main->bindWidget("player", player);

	// Events from MediaScanner
	// TODO: only if admin
	enableUpdates(true);
	std::string sessionId = this->sessionId();
	_scanner.scanComplete().connect(std::bind([=] (Scanner::MediaScanner::Stats stats)
	{
		Wt::WServer::instance()->post(sessionId, [=]
		{
			notify(Wt::WString::tr("msg-notify-scan-complete")
					.arg(stats.nbFiles())
					.arg(stats.additions)
					.arg(stats.deletions)
					.arg(stats.nbDuplicates())
					.arg(stats.nbErrors()));
			triggerUpdate();
		});
//		notify("TEST");
	}, std::placeholders::_1));


	internalPathChanged().connect(std::bind([=]
	{
		handlePathChange(mainStack);
	}));

	handlePathChange(mainStack);
}

void
LmsApplication::notify(const Wt::WString& message)
{
	LMS_LOG(UI, INFO) << "Notifying message '" << message.toUTF8() << "'";
	root()->addWidget(new Wt::WText(message));
}

void notify(const Wt::WString& message)
{
	LmsApplication::instance()->notify(message);
}

} // namespace UserInterface


