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
#include "MediaPlayer.hpp"
#include "PlayHistoryView.hpp"
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
LmsApplication::create(const Wt::WEnvironment& env, Wt::Dbo::SqlConnectionPool& connectionPool, LmsApplicationGroupContainer& appGroups, Scanner::MediaScanner& scanner)
{
	/*
	 * You could read information from the environment to decide whether
	 * the user has permission to start a new application
	 */
	return std::make_unique<LmsApplication>(env, connectionPool, appGroups, scanner);
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
LmsApplication::LmsApplication(const Wt::WEnvironment& env, Wt::Dbo::SqlConnectionPool& connectionPool, LmsApplicationGroupContainer& appGroups, Scanner::MediaScanner& scanner)
: Wt::WApplication(env),
  _db(connectionPool),
  _appGroups(appGroups),
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
	messageResourceBundle().use(appRoot() + "artistsinfo");
	messageResourceBundle().use(appRoot() + "explore");
	messageResourceBundle().use(appRoot() + "login");
	messageResourceBundle().use(appRoot() + "mediaplayer");
	messageResourceBundle().use(appRoot() + "messages");
	messageResourceBundle().use(appRoot() + "playqueue");
	messageResourceBundle().use(appRoot() + "playhistory");
	messageResourceBundle().use(appRoot() + "release");
	messageResourceBundle().use(appRoot() + "releases");
	messageResourceBundle().use(appRoot() + "releasesinfo");
	messageResourceBundle().use(appRoot() + "settings");
	messageResourceBundle().use(appRoot() + "templates");
	messageResourceBundle().use(appRoot() + "tracks");
	messageResourceBundle().use(appRoot() + "tracksinfo");

	// Require js here to avoid async problems
	requireJQuery("/js/jquery-1.10.2.min.js");
	require("/js/mediaplayer.js");
	require("/js/bootstrap-notify.js");

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
		LmsApp->getDb().getLogin().changed().connect(this, &LmsApplication::handleAuthEvent);
		_auth = root()->addNew<Auth>();
	}
}

void
LmsApplication::finalize()
{
	LmsApplicationInfo info = LmsApplicationInfo::fromEnvironment(environment());

	getApplicationGroup().postOthers([info]
	{
		LmsApp->getEvents().appClosed(info);
	});

	getApplicationGroup().leave();

	preQuit().emit();
}

Wt::WLink
LmsApplication::createArtistLink(Database::Artist::pointer artist)
{
	return Wt::WLink(Wt::LinkType::InternalPath, "/artist/" + std::to_string(artist.id()));
}

std::unique_ptr<Wt::WAnchor>
LmsApplication::createArtistAnchor(Database::Artist::pointer artist, bool addText)
{
	auto res = std::make_unique<Wt::WAnchor>(createArtistLink(artist));

	if (addText)
	{
		res->setTextFormat(Wt::TextFormat::Plain);
		res->setText(Wt::WString::fromUTF8(artist->getName()));
	}

	return res;
}

Wt::WLink
LmsApplication::createReleaseLink(Database::Release::pointer release)
{
	return Wt::WLink(Wt::LinkType::InternalPath, "/release/" + std::to_string(release.id()));
}

std::unique_ptr<Wt::WAnchor>
LmsApplication::createReleaseAnchor(Database::Release::pointer release, bool addText)
{
	auto res = std::make_unique<Wt::WAnchor>(createReleaseLink(release));

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
	res->setToolTip(cluster->getType()->getName(), Wt::TextFormat::Plain);
	res->setInline(true);

	return res;
}

void
LmsApplication::goHome()
{
	setInternalPath("/artists", true);
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
	IdxExplore	= 0,
	IdxPlayQueue,
	IdxPlayHistory,
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
		{ "/artists",		IdxExplore,		false },
		{ "/artist",		IdxExplore,		false },
		{ "/releases",		IdxExplore,		false },
		{ "/release",		IdxExplore,		false },
		{ "/tracks",		IdxExplore,		false },
		{ "/playqueue",		IdxPlayQueue,		false },
		{ "/playhistory",	IdxPlayHistory,		false },
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

	wApp->setInternalPath("/artists", true);
}

LmsApplicationGroup&
LmsApplication::getApplicationGroup()
{
	return _appGroups.get(_userIdentity);
}

void
LmsApplication::handleAuthEvent()
{
	try
	{
		if (!getDb().getLogin().loggedIn())
		{
			LMS_LOG(UI, INFO) << "User '" << _userIdentity << " 'logged out, session = " << sessionId();

			goHomeAndQuit();
			return;
		}
		else
		{
			_userIdentity = getAuthUser().identity(Wt::Auth::Identity::LoginName);
			LmsApplicationInfo info = LmsApplicationInfo::fromEnvironment(environment());

			LMS_LOG(UI, INFO) << "User '" << _userIdentity << "' logged in from '" << environment().clientAddress() << "', user agent = " << environment().userAgent() << ", session = " <<  sessionId();
			getApplicationGroup().join(info);

			getApplicationGroup().postOthers([info]
			{
				LmsApp->getEvents().appOpen(info);
			});

			createHome();
		}
	}
	catch (std::exception& e)
	{
		LMS_LOG(UI, ERROR) << "Error while handling auth event: " << e.what();
		throw LmsException("Internal error"); // Do not put details here at it appears on the user rendered html
	}
}

void
LmsApplication::createHome()
{
	// Handle Media Scanner events and other session events
	enableUpdates(true);

	{
		Wt::Dbo::Transaction transaction (LmsApp->getDboSession());
		_isAdmin = LmsApp->getUser()->isAdmin();
	}

	_imageResource = std::make_shared<ImageResource>(_db);
	_transcodeResource = std::make_shared<TranscodeResource>(_db);

	setConfirmCloseMessage(Wt::WString::tr("Lms.quit-confirm"));

	Wt::WTemplate* main = root()->addWidget(std::make_unique<Wt::WTemplate>(Wt::WString::tr("Lms.template")));

	// Navbar
	Wt::WNavigationBar* navbar = main->bindNew<Wt::WNavigationBar>("navbar-top");
	navbar->setTitle("LMS", Wt::WLink(Wt::LinkType::InternalPath, "/artists"));
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
	{
		auto menuItem = menu->insertItem(4, Wt::WString::tr("Lms.PlayHistory.playhistory"));
		menuItem->setLink(Wt::WLink(Wt::LinkType::InternalPath, "/playhistory"));
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

	Explore* explore = mainStack->addNew<Explore>();
	PlayQueue* playqueue = mainStack->addNew<PlayQueue>();
	mainStack->addNew<PlayHistory>();
	mainStack->addNew<SettingsView>();

	// Admin stuff
	if (_isAdmin)
	{
		mainStack->addNew<DatabaseSettingsView>();
		mainStack->addNew<UsersView>();
		mainStack->addNew<UserView>();
	}

	explore->tracksAdd.connect([=] (std::vector<Database::Track::pointer> tracks)
	{
		playqueue->addTracks(tracks);
	});

	explore->tracksPlay.connect([=] (std::vector<Database::Track::pointer> tracks)
	{
		playqueue->playTracks(tracks);
	});


	// MediaPlayer
	MediaPlayer* player = main->bindNew<MediaPlayer>("player");

	// Events from MediaPlayer
	player->playNext.connect([=]
	{
		playqueue->playNext();
	});
	player->playPrevious.connect([=]
	{
		playqueue->playPrevious();
	});
	player->playbackEnded.connect([=]
	{
		playqueue->playNext();
	});

	playqueue->loadTrack.connect([=] (Database::IdType trackId, bool play)
	{
		_events.lastLoadedTrackId = trackId;
		_events.trackLoaded(trackId, play);
	});

	playqueue->trackUnload.connect([=]
	{
		_events.lastLoadedTrackId.reset();
		_events.trackUnloaded();
	});

	// Events from MediaScanner
	std::string sessionId = LmsApp->sessionId();
	_scanner.scanComplete().connect([=] (Scanner::MediaScanner::Stats stats)
	{
		// Runs from media scanner context
		Wt::WServer::instance()->post(sessionId, [=]
		{
			_events.dbScanned.emit(stats);
			triggerUpdate();
		});
	});

	_events.dbScanned.connect([=] (Scanner::MediaScanner::Stats stats)
	{
		if (_isAdmin)
		{
			notifyMsg(MsgType::Info, Wt::WString::tr("Lms.Admin.Database.scan-complete")
				.arg(static_cast<unsigned>(stats.nbFiles()))
				.arg(static_cast<unsigned>(stats.additions))
				.arg(static_cast<unsigned>(stats.updates))
				.arg(static_cast<unsigned>(stats.deletions))
				.arg(static_cast<unsigned>(stats.nbDuplicates()))
				.arg(static_cast<unsigned>(stats.nbErrors())));
		}
	});

	// Events from Application group
	_events.appOpen.connect([=] (LmsApplicationInfo info)
	{
		// Only one active session by user
		if (!LmsApp->getUser()->isDemo())
		{
			setConfirmCloseMessage("");
			quit(Wt::WString::tr("Lms.quit-other-session"));
		}
	});

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
		throw LmsException("Internal error"); // Do not put details here at it appears on the user rendered html
	}
}

static std::string msgTypeToString(MsgType type)
{
	switch(type)
	{
		case MsgType::Success:	return "success";
		case MsgType::Info:	return "info";
		case MsgType::Warning:	return "warning";
		case MsgType::Danger:	return "danger";
	}
	return "";
}

void
LmsApplication::post(std::function<void()> func)
{
	Wt::WServer::instance()->post(LmsApp->sessionId(), func);
}

void
LmsApplication::notifyMsg(MsgType type, const Wt::WString& message, std::chrono::milliseconds duration)
{
	LMS_LOG(UI, INFO) << "Notifying message '" << message.toUTF8() << "' of type '" << msgTypeToString(type);

	std::ostringstream oss;

	oss << "$.notify({"
			"message: '" << message.toUTF8() << "'"
		"},{"
			"type: '" << msgTypeToString(type) << "',"
			"placement: {from: 'top', align: 'center'},"
			"timer: 250,"
			"delay: " << duration.count() << ""
		"});";

	LmsApp->doJavaScript(oss.str());
}


} // namespace UserInterface


