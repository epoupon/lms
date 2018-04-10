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

#include <Wt/WAnchor>
#include <Wt/WBootstrapTheme>
#include <Wt/WEnvironment>
#include <Wt/WMenu>
#include <Wt/WNavigationBar>
#include <Wt/WPopupMenu>
#include <Wt/WStackedWidget>
#include <Wt/WText>
#include <Wt/Auth/Identity>

#include "config/config.h"
#include "utils/Logger.hpp"
#include "utils/Utils.hpp"

#include "explore/Explore.hpp"
#include "HomeView.hpp"
#include "MediaPlayer.hpp"
#include "PlayQueueView.hpp"
#include "SettingsView.hpp"

#include "admin/InitWizardView.hpp"
#include "admin/DatabaseSettingsView.hpp"
#include "admin/UserView.hpp"
#include "admin/UsersView.hpp"

#include "resource/ImageResource.hpp"
#include "resource/TranscodeResource.hpp"

#include "LmsApplication.hpp"

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
	messageResourceBundle().use(appRoot() + "admin-database");
	messageResourceBundle().use(appRoot() + "admin-user");
	messageResourceBundle().use(appRoot() + "admin-users");
	messageResourceBundle().use(appRoot() + "admin-wizard");
	messageResourceBundle().use(appRoot() + "artist");
	messageResourceBundle().use(appRoot() + "artists");
	messageResourceBundle().use(appRoot() + "home");
	messageResourceBundle().use(appRoot() + "explore");
	messageResourceBundle().use(appRoot() + "login");
	messageResourceBundle().use(appRoot() + "mediaplayer");
	messageResourceBundle().use(appRoot() + "messages");
	messageResourceBundle().use(appRoot() + "playqueue");
	messageResourceBundle().use(appRoot() + "release");
	messageResourceBundle().use(appRoot() + "releases");
	messageResourceBundle().use(appRoot() + "settings");
	messageResourceBundle().use(appRoot() + "tracks");
	messageResourceBundle().use(appRoot() + "templates");

	// Require js here to avoid async problems
	require("/js/mediaplayer.js");

	setTitle("LMS");

	// If here is no account in the database, launch the first connection wizard
	bool firstConnection;
	{
		Wt::Dbo::Transaction transaction(DboSession());

		firstConnection = (Database::User::getAll(DboSession()).size() == 0);
	}

	LMS_LOG(UI, DEBUG) << "Creating root widget. First connection = " << std::boolalpha << firstConnection;

	if (firstConnection)
	{
		root()->addWidget(new InitWizardView());
	}
	else
	{
		DbHandler().getLogin().changed().connect(std::bind([=]
		{
			try
			{
				handleAuthEvent();
			}
			catch (std::exception& e)
			{
				LMS_LOG(UI, ERROR) << "Error while handling auth event: " << e.what();
				throw std::runtime_error("Internal error");
			}
		}));

		_auth = new Auth();
		root()->addWidget(_auth);
	}
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

Scanner::MediaScanner& MediaScanner()
{
	return LmsApplication::instance()->getMediaScanner();
}

Wt::WAnchor*
LmsApplication::createArtistAnchor(Database::Artist::pointer artist, bool addText)
{
	auto res = new Wt::WAnchor(Wt::WLink(Wt::WLink::InternalPath, "/artist/" + std::to_string(artist.id())));

	if (addText)
	{
		res->setTextFormat(Wt::PlainText);
		res->setText(Wt::WString::fromUTF8(artist->getName()));
	}

	return res;
}

Wt::WAnchor*
LmsApplication::createReleaseAnchor(Database::Release::pointer release, bool addText)
{
	auto res = new Wt::WAnchor(Wt::WLink(Wt::WLink::InternalPath, "/release/" + std::to_string(release.id())));

	if (addText)
	{
		res->setTextFormat(Wt::PlainText);
		res->setText(Wt::WString::fromUTF8(release->getName()));
	}

	return res;
}

void
LmsApplication::goHome()
{
	setInternalPath("/home", true);
}

void
LmsApplication::goHomeAndQuit()
{
	setConfirmCloseMessage("");
	WApplication::quit("");
	redirect("/");
}

enum IdxRoot
{
	IdxHome		= 0,
	IdxExplore,
	IdxPlayQueue,
	IdxSettings,
	IdxAdminDatabase,
	IdxAdminUsers,
	IdxAdminUser,
};

static void
handlePathChange(Wt::WStackedWidget* stack, bool isAdmin)
{
	static const struct
	{
		std::string path;
		int index;
		bool admin;
	} views[] =
	{
		{ "/home",		IdxHome,		false },
		{ "/artists",		IdxExplore,		false },
		{ "/artist",		IdxExplore,		false },
		{ "/releases",		IdxExplore,		false },
		{ "/release",		IdxExplore,		false },
		{ "/tracks",		IdxExplore,		false },
		{ "/playqueue",		IdxPlayQueue,		false },
		{ "/settings",		IdxSettings,		false },
		{ "/admin/database",	IdxAdminDatabase,	true },
		{ "/admin/users",	IdxAdminUsers,		true },
		{ "/admin/user",	IdxAdminUser,		true },
	};

	LMS_LOG(UI, DEBUG) << "Internal path changed to '" << wApp->internalPath() << "'";

	for (auto& view : views)
	{
		if (wApp->internalPathMatches(view.path))
		{
			if (view.admin && !isAdmin)
				break;

			stack->setCurrentIndex(view.index);
			return;
		}
	}

	wApp->setInternalPath("/home", true);
}

void
LmsApplication::handleAuthEvent(void)
{
	if (!DbHandler().getLogin().loggedIn())
	{
		LMS_LOG(UI, INFO) << "User logged out, session = " << Wt::WApplication::instance()->sessionId();

		goHomeAndQuit();
		return;
	}

	LMS_LOG(UI, INFO) << "User '" << CurrentAuthUser().identity(Wt::Auth::Identity::LoginName) << "' logged in from '" << Wt::WApplication::instance()->environment().clientAddress() << "', user agent = " << Wt::WApplication::instance()->environment().agent() << ", session = " <<  Wt::WApplication::instance()->sessionId();

	{
		Wt::Dbo::Transaction transaction (DboSession());
		_isAdmin = CurrentUser()->isAdmin();
	}

	_imageResource = new ImageResource(_db, root());
	_transcodeResource = new TranscodeResource(_db, root());

	setConfirmCloseMessage(Wt::WString::tr("Lms.quit-confirm"));

	auto main = new Wt::WTemplate(Wt::WString::tr("Lms.template"), root());

	// Navbar
	auto navbar = new Wt::WNavigationBar();
	navbar->setTitle("LMS", Wt::WLink(Wt::WLink::InternalPath, "/home"));
	navbar->setResponsive(true);

	main->bindWidget("navbar-top", navbar);

	auto menu = new Wt::WMenu();
	{
		auto menuItem = menu->insertItem(0, Wt::WString::tr("Lms.Explore.artists"));
		menuItem->setLink(Wt::WLink(Wt::WLink::InternalPath, "/artists"));
		menuItem->setSelectable(false);
	}
	{
		auto menuItem = menu->insertItem(1, Wt::WString::tr("Lms.Explore.releases"));
		menuItem->setLink(Wt::WLink(Wt::WLink::InternalPath, "/releases"));
		menuItem->setSelectable(false);
	}
	{
		auto menuItem = menu->insertItem(2, Wt::WString::tr("Lms.Explore.tracks"));
		menuItem->setLink(Wt::WLink(Wt::WLink::InternalPath, "/tracks"));
		menuItem->setSelectable(false);
	}
	{
		auto menuItem = menu->insertItem(3, Wt::WString::tr("Lms.PlayQueue.playqueue"));
		menuItem->setLink(Wt::WLink(Wt::WLink::InternalPath, "/playqueue"));
		menuItem->setSelectable(false);
	}
	navbar->addMenu(menu);

	auto rightMenu = new Wt::WMenu();
	std::size_t itemCounter = 0;

	if (_isAdmin)
	{
		auto menuItem = rightMenu->insertItem(itemCounter++, Wt::WString::tr("Lms.administration"));
		menuItem->setSelectable(false);

		Wt::WPopupMenu *admin = new Wt::WPopupMenu();
		auto dbSettings = admin->insertItem(0, Wt::WString::tr("Lms.Admin.Database.database"));
		dbSettings->setLink(Wt::WLink(Wt::WLink::InternalPath, "/admin/database"));
		dbSettings->setSelectable(false);

		auto usersSettings = admin->insertItem(1, Wt::WString::tr("Lms.Admin.Users.users"));
		usersSettings->setLink(Wt::WLink(Wt::WLink::InternalPath, "/admin/users"));
		usersSettings->setSelectable(false);

		menuItem->setMenu(admin);
	}

	{
		auto menuItem = rightMenu->insertItem(itemCounter++, Wt::WString::tr("Lms.Settings.settings"));
		menuItem->setLink(Wt::WLink(Wt::WLink::InternalPath, "/settings"));
		menuItem->setSelectable(false);
	}
	{
		auto menuItem = rightMenu->insertItem(itemCounter++, Wt::WString::tr("Lms.logout"));
		menuItem->setSelectable(true);
		menuItem->triggered().connect(std::bind([=]
		{
			setConfirmCloseMessage("");
			_auth->logout();
		}));
	}
	navbar->addMenu(rightMenu, Wt::AlignRight);

	// Contents
	Wt::WStackedWidget* mainStack = new Wt::WStackedWidget();
	main->bindWidget("contents", mainStack);

	mainStack->addWidget(new Home());

	auto explore = new Explore();
	mainStack->addWidget(explore);

	auto playqueue = new PlayQueue();
	mainStack->addWidget(playqueue);

	auto settings = new SettingsView();
	mainStack->addWidget(settings);

	// Admin stuff
	if (_isAdmin)
	{
		auto databaseSettings = new DatabaseSettingsView();
		mainStack->addWidget(databaseSettings);

		auto users = new UsersView();
		mainStack->addWidget(users);

		auto user = new UserView();
		mainStack->addWidget(user);
	}

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

	// Events from MediaPlayer
	player->playNext.connect(std::bind([=]
	{
		playqueue->playNext();
	}));
	player->playPrevious.connect(std::bind([=]
	{
		playqueue->playPrevious();
	}));
	player->playbackEnded.connect(std::bind([=]
	{
		playqueue->playNext();
	}));

	// Events from the PlayQueue
	playqueue->playTrack.connect(player, &MediaPlayer::playTrack);
	playqueue->playbackStop.connect(player, &MediaPlayer::stop);

	// Events from MediaScanner
	if (_isAdmin)
	{
		enableUpdates(true);
		std::string sessionId = this->sessionId();
		_scanner.scanComplete().connect(std::bind([=] (Scanner::MediaScanner::Stats stats)
		{
			Wt::WServer::instance()->post(sessionId, [=]
			{
				notifyMsg(Wt::WString::tr("Lms.Admin.Database.scan-complete")
					.arg(stats.nbFiles())
					.arg(stats.additions)
					.arg(stats.deletions)
					.arg(stats.nbDuplicates())
					.arg(stats.nbErrors()));
				triggerUpdate();
			});
		}, std::placeholders::_1));
	}

	internalPathChanged().connect(std::bind([=]
	{
		handlePathChange(mainStack, _isAdmin);
	}));

	handlePathChange(mainStack, _isAdmin);
}

void
LmsApplication::notify(const Wt::WEvent& event)
{
	try
	{
		WApplication::notify(event);
	}
	catch (std::exception& e)
	{
		LMS_LOG(UI, ERROR) << "Caught exception: " << e.what();
		throw std::runtime_error("Internal error");
	}
}

void
LmsApplication::notifyMsg(const Wt::WString& message)
{
	LMS_LOG(UI, INFO) << "Notifying message '" << message.toUTF8() << "'";
	root()->addWidget(new Wt::WText(message));
}

} // namespace UserInterface


