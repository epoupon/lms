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

#include "LmsApplication.hpp"

#include <Wt/WAnchor.h>
#include <Wt/WBootstrapTheme.h>
#include <Wt/WEnvironment.h>
#include <Wt/WMenu.h>
#include <Wt/WNavigationBar.h>
#include <Wt/WPopupMenu.h>
#include <Wt/WServer.h>
#include <Wt/WStackedWidget.h>
#include <Wt/WText.h>
#include <Wt/Auth/Identity.h>

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

namespace UserInterface {

std::unique_ptr<Wt::WApplication>
LmsApplication::create(const Wt::WEnvironment& env, Wt::Dbo::SqlConnectionPool& connectionPool, Scanner::MediaScanner& scanner)
{
	/*
	 * You could read information from the environment to decide whether
	 * the user has permission to start a new application
	 */
	return std::make_unique<LmsApplication>(env, connectionPool, scanner);
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
	auto  bootstrapTheme = std::make_unique<Wt::WBootstrapTheme>();
	bootstrapTheme->setVersion(Wt::BootstrapVersion::v3);
	bootstrapTheme->setResponsive(true);
	setTheme(std::move(bootstrapTheme));

	useStyleSheet("css/lms.css");
	useStyleSheet("resources/font-awesome/css/font-awesome.min.css");

	// Add a resource bundle
	messageResourceBundle().use(appRoot() + "admin-database");
	messageResourceBundle().use(appRoot() + "admin-user");
	messageResourceBundle().use(appRoot() + "admin-users");
	messageResourceBundle().use(appRoot() + "admin-initwizard");
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
		Wt::Dbo::Transaction transaction(LmsApp->getDboSession());

		firstConnection = (Database::User::getAll(LmsApp->getDboSession()).size() == 0);
	}

	LMS_LOG(UI, DEBUG) << "Creating root widget. First connection = " << firstConnection;

	if (firstConnection)
	{
		root()->addWidget(std::make_unique<InitWizardView>());
	}
	else
	{
		LmsApp->getDb().getLogin().changed().connect(std::bind([=]
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

		_auth = root()->addNew<Auth>();
	}
}

std::unique_ptr<Wt::WAnchor>
LmsApplication::createArtistAnchor(Database::Artist::pointer artist, bool addText)
{
	auto res = std::make_unique<Wt::WAnchor>(Wt::WLink(Wt::LinkType::InternalPath, "/artist/" + std::to_string(artist.id())));

	if (addText)
	{
		res->setTextFormat(Wt::TextFormat::Plain);
		res->setText(Wt::WString::fromUTF8(artist->getName()));
	}

	return res;
}

std::unique_ptr<Wt::WAnchor>
LmsApplication::createReleaseAnchor(Database::Release::pointer release, bool addText)
{
	auto res = std::make_unique<Wt::WAnchor>(Wt::WLink(Wt::LinkType::InternalPath, "/release/" + std::to_string(release.id())));

	if (addText)
	{
		res->setTextFormat(Wt::TextFormat::Plain);
		res->setText(Wt::WString::fromUTF8(release->getName()));
	}

	return res;
}

std::unique_ptr<Wt::WTemplate>
LmsApplication::createCluster(Database::Cluster::pointer cluster, bool canDelete)
{
	std::string styleClass = "Lms-cluster-type-" + std::to_string(cluster->getType().id() % 10);

	auto res = std::make_unique<Wt::WTemplate>(Wt::WString::tr("Lms.Explore.template.cluster-entry").arg(styleClass));
	res->addFunction("tr", &Wt::WTemplate::Functions::tr);

	res->bindString("name", Wt::WString::fromUTF8(cluster->getName()), Wt::TextFormat::Plain);
	res->setCondition("if-can-delete", canDelete);

	res->setStyleClass("Lms-cluster");

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
	if (!LmsApp->getDb().getLogin().loggedIn())
	{
		LMS_LOG(UI, INFO) << "User logged out, session = " << Wt::WApplication::instance()->sessionId();

		goHomeAndQuit();
		return;
	}

	LMS_LOG(UI, INFO) << "User '" << LmsApp->getCurrentAuthUser().identity(Wt::Auth::Identity::LoginName) << "' logged in from '" << Wt::WApplication::instance()->environment().clientAddress() << "', user agent = " << Wt::WApplication::instance()->environment().userAgent() << ", session = " <<  Wt::WApplication::instance()->sessionId();

	{
		Wt::Dbo::Transaction transaction (LmsApp->getDboSession());
		_isAdmin = LmsApp->getCurrentUser()->isAdmin();
	}

	_imageResource = std::make_shared<ImageResource>(_db);
	_transcodeResource = std::make_shared<TranscodeResource>(_db);

	setConfirmCloseMessage(Wt::WString::tr("Lms.quit-confirm"));

	Wt::WTemplate* main = root()->addWidget(std::make_unique<Wt::WTemplate>(Wt::WString::tr("Lms.template")));

	// Navbar
	Wt::WNavigationBar* navbar = main->bindNew<Wt::WNavigationBar>("navbar-top");
	navbar->setTitle("LMS", Wt::WLink(Wt::LinkType::InternalPath, "/home"));
	navbar->setResponsive(true);

	Wt::WMenu* menu = navbar->addMenu(std::make_unique<Wt::WMenu>());
	{
		auto menuItem = menu->insertItem(0, Wt::WString::tr("Lms.Explore.artists"));
		menuItem->setLink(Wt::WLink(Wt::LinkType::InternalPath, "/artists"));
		menuItem->setSelectable(false);
	}
	{
		auto menuItem = menu->insertItem(1, Wt::WString::tr("Lms.Explore.releases"));
		menuItem->setLink(Wt::WLink(Wt::LinkType::InternalPath, "/releases"));
		menuItem->setSelectable(false);
	}
	{
		auto menuItem = menu->insertItem(2, Wt::WString::tr("Lms.Explore.tracks"));
		menuItem->setLink(Wt::WLink(Wt::LinkType::InternalPath, "/tracks"));
		menuItem->setSelectable(false);
	}
	{
		auto menuItem = menu->insertItem(3, Wt::WString::tr("Lms.PlayQueue.playqueue"));
		menuItem->setLink(Wt::WLink(Wt::LinkType::InternalPath, "/playqueue"));
		menuItem->setSelectable(false);
	}

	Wt::WMenu* rightMenu = navbar->addMenu(std::make_unique<Wt::WMenu>(), Wt::AlignmentFlag::Right);
	std::size_t itemCounter = 0;
	if (_isAdmin)
	{
		auto menuItem = rightMenu->insertItem(itemCounter++, Wt::WString::tr("Lms.administration"));
		menuItem->setSelectable(false);

		auto admin = std::make_unique<Wt::WPopupMenu>();
		auto dbSettings = admin->insertItem(0, Wt::WString::tr("Lms.Admin.Database.database"));
		dbSettings->setLink(Wt::WLink(Wt::LinkType::InternalPath, "/admin/database"));
		dbSettings->setSelectable(false);

		auto usersSettings = admin->insertItem(1, Wt::WString::tr("Lms.Admin.Users.users"));
		usersSettings->setLink(Wt::WLink(Wt::LinkType::InternalPath, "/admin/users"));
		usersSettings->setSelectable(false);

		menuItem->setMenu(std::move(admin));
	}

	{
		auto menuItem = rightMenu->insertItem(itemCounter++, Wt::WString::tr("Lms.Settings.settings"));
		menuItem->setLink(Wt::WLink(Wt::LinkType::InternalPath, "/settings"));
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

	// Contents
	// Order is important in mainStack, see IdxRoot!
	Wt::WStackedWidget* mainStack = main->bindNew<Wt::WStackedWidget>("contents");

	mainStack->addNew<Home>();
	Explore* explore = mainStack->addNew<Explore>();
	PlayQueue* playqueue = mainStack->addNew<PlayQueue>();
	mainStack->addNew<SettingsView>();

	// Admin stuff
	if (_isAdmin)
	{
		mainStack->addNew<DatabaseSettingsView>();
		mainStack->addNew<UsersView>();
		mainStack->addNew<UserView>();
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
	MediaPlayer* player = main->bindNew<MediaPlayer>("player");

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
					.arg(static_cast<unsigned>(stats.nbFiles()))
					.arg(static_cast<unsigned>(stats.additions))
					.arg(static_cast<unsigned>(stats.deletions))
					.arg(static_cast<unsigned>(stats.nbDuplicates()))
					.arg(static_cast<unsigned>(stats.nbErrors())));
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
	root()->addNew<Wt::WText>(message);
}

} // namespace UserInterface


