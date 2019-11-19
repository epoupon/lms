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

#include "config/config.h"
#include "cover/CoverArtGrabber.hpp"
#include "database/Artist.hpp"
#include "database/Cluster.hpp"
#include "database/Db.hpp"
#include "database/Release.hpp"
#include "database/User.hpp"
#include "explore/Explore.hpp"
#include "utils/Logger.hpp"
#include "utils/Service.hpp"
#include "utils/Utils.hpp"

#include "admin/InitWizardView.hpp"
#include "admin/DatabaseSettingsView.hpp"
#include "admin/UserView.hpp"
#include "admin/UsersView.hpp"
#include "resource/ImageResource.hpp"
#include "resource/AudioResource.hpp"
#include "Auth.hpp"
#include "LmsApplicationException.hpp"
#include "MediaPlayer.hpp"
#include "PlayHistoryView.hpp"
#include "PlayQueueView.hpp"
#include "SettingsView.hpp"


namespace UserInterface {

std::unique_ptr<Wt::WApplication>
LmsApplication::create(const Wt::WEnvironment& env, Database::Db& db, LmsApplicationGroupContainer& appGroups)
{
	return std::make_unique<LmsApplication>(env, db.createSession(), appGroups);
}

LmsApplication*
LmsApplication::instance()
{
	return reinterpret_cast<LmsApplication*>(Wt::WApplication::instance());
}

Wt::Dbo::ptr<Database::User>
LmsApplication::getUser() const
{
	if (!_userId)
		return {};

	return Database::User::getById(*_dbSession, *_userId);
}

bool
LmsApplication::isUserAuthStrong() const
{
	return *_userAuthStrong;
}

bool
LmsApplication::isUserAdmin() const
{
	auto transaction {_dbSession->createSharedTransaction()};

	return getUser()->isAdmin();
}

bool
LmsApplication::isUserDemo() const
{
	auto transaction {_dbSession->createSharedTransaction()};

	return getUser()->isDemo();
}

std::string
LmsApplication::getUserLoginName() const
{
	auto transaction {_dbSession->createSharedTransaction()};

	return getUser()->getLoginName();
}

LmsApplication::LmsApplication(const Wt::WEnvironment& env,
		std::unique_ptr<Database::Session> dbSession,
		LmsApplicationGroupContainer& appGroups)
: Wt::WApplication {env},
  _dbSession {std::move(dbSession)},
  _appGroups {appGroups}
{
	auto  bootstrapTheme = std::make_unique<Wt::WBootstrapTheme>();
	bootstrapTheme->setVersion(Wt::BootstrapVersion::v3);
	bootstrapTheme->setResponsive(true);
	setTheme(std::move(bootstrapTheme));

	addMetaHeader(Wt::MetaHeaderType::Meta, "viewport", "width=device-width, user-scalable=no");

	useStyleSheet("css/lms.css");
	useStyleSheet("resources/font-awesome/css/font-awesome.min.css");

	// Add a resource bundle
	messageResourceBundle().use(appRoot() + "admin-database");
	messageResourceBundle().use(appRoot() + "admin-user");
	messageResourceBundle().use(appRoot() + "admin-users");
	messageResourceBundle().use(appRoot() + "admin-initwizard");
	messageResourceBundle().use(appRoot() + "artist");
	messageResourceBundle().use(appRoot() + "artistinfo");
	messageResourceBundle().use(appRoot() + "artistlink");
	messageResourceBundle().use(appRoot() + "artists");
	messageResourceBundle().use(appRoot() + "artistsinfo");
	messageResourceBundle().use(appRoot() + "error");
	messageResourceBundle().use(appRoot() + "explore");
	messageResourceBundle().use(appRoot() + "login");
	messageResourceBundle().use(appRoot() + "mediaplayer");
	messageResourceBundle().use(appRoot() + "messages");
	messageResourceBundle().use(appRoot() + "playqueue");
	messageResourceBundle().use(appRoot() + "playhistory");
	messageResourceBundle().use(appRoot() + "release");
	messageResourceBundle().use(appRoot() + "releaseinfo");
	messageResourceBundle().use(appRoot() + "releaselink");
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

	// Handle Media Scanner events and other session events
	enableUpdates(true);

	// If here is no account in the database, launch the first connection wizard
	bool firstConnection {};
	{
		auto transaction {_dbSession->createSharedTransaction()};
		firstConnection = Database::User::getAll(*_dbSession).empty();
	}

	LMS_LOG(UI, DEBUG) << "Creating root widget. First connection = " << firstConnection;

	if (firstConnection)
	{
		root()->addWidget(std::make_unique<InitWizardView>());
		return;
	}

	const auto userId {processAuthToken(env)};
	if (userId)
	{
		try
		{
			handleUserLoggedIn(*userId, false);
		}
		catch (LmsApplicationException& e)
		{
			LMS_LOG(UI, WARNING) << "Caught a LmsApplication exception: " << e.what();
			handleException(e);
		}
		catch (std::exception& e)
		{
			LMS_LOG(UI, ERROR) << "Caught exception: " << e.what();
			throw LmsException {"Internal error"}; // Do not put details here at it may appear on the user rendered html
		}
	}
	else
	{
		Auth* auth {root()->addNew<Auth>()};
		auth->userLoggedIn.connect(this, [this](Database::IdType userId)
		{
			handleUserLoggedIn(userId, true);
		});
	}
}

void
LmsApplication::finalize()
{
	if (_userId)
	{
		LmsApplicationInfo info = LmsApplicationInfo::fromEnvironment(environment());

		getApplicationGroup().postOthers([info]
		{
			LmsApp->getEvents().appClosed(info);
		});

		getApplicationGroup().leave();
	}

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
LmsApplication::handleException(LmsApplicationException& e)
{
	root()->clear();
	Wt::WTemplate* t {root()->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Error.template"))};
	t->addFunction("tr", &Wt::WTemplate::Functions::tr);

	t->bindString("error", e.what());
	Wt::WPushButton* btn {t->bindNew<Wt::WPushButton>("btn-go-home", Wt::WString::tr("Lms.Error.go-home"))};
	btn->clicked().connect([this]()
	{
		setConfirmCloseMessage("");
		redirect("/");
	});
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

	for (const auto& view : views)
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
	return _appGroups.get(*_userId);
}

void
LmsApplication::handleUserLoggedOut()
{
	LMS_LOG(UI, INFO) << "User '" << getUserLoginName() << " 'logged out";

	{
		auto transaction {_dbSession->createUniqueTransaction()};
		getUser().modify()->clearAuthTokens();
	}

	setConfirmCloseMessage("");
	goHomeAndQuit();
}

void
LmsApplication::handleUserLoggedIn(Database::IdType userId, bool strongAuth)
{
	_userId = userId;
	_userAuthStrong = strongAuth;

	root()->clear();

	const LmsApplicationInfo info {LmsApplicationInfo::fromEnvironment(environment())};

	LMS_LOG(UI, INFO) << "User '" << getUserLoginName() << "' logged in from '" << environment().clientAddress() << "', user agent = " << environment().userAgent();
	getApplicationGroup().join(info);

	getApplicationGroup().postOthers([info]
	{
		LmsApp->getEvents().appOpen(info);
	});

	createHome();
}

void
LmsApplication::createHome()
{
	_imageResource = std::make_shared<ImageResource>();
	_audioResource = std::make_shared<AudioResource>();

	setConfirmCloseMessage(Wt::WString::tr("Lms.quit-confirm"));

	Wt::WTemplate* main {root()->addWidget(std::make_unique<Wt::WTemplate>(Wt::WString::tr("Lms.template")))};

	// Navbar
	Wt::WNavigationBar* navbar {main->bindNew<Wt::WNavigationBar>("navbar-top")};
	navbar->setTitle("LMS", Wt::WLink {Wt::LinkType::InternalPath, "/artists"});
	navbar->setResponsive(true);

	Wt::WMenu* menu {navbar->addMenu(std::make_unique<Wt::WMenu>())};
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
	if (isUserAdmin())
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
		menuItem->triggered().connect(this, &LmsApplication::handleUserLoggedOut);
	}

	// Contents
	// Order is important in mainStack, see IdxRoot!
	Wt::WStackedWidget* mainStack = main->bindNew<Wt::WStackedWidget>("contents");

	Explore* explore = mainStack->addNew<Explore>();
	PlayQueue* playqueue = mainStack->addNew<PlayQueue>();
	mainStack->addNew<PlayHistory>();
	mainStack->addNew<SettingsView>();

	// Admin stuff
	if (isUserAdmin())
	{
		mainStack->addNew<DatabaseSettingsView>();
		mainStack->addNew<UsersView>();
		mainStack->addNew<UserView>();
	}

	explore->tracksAdd.connect([=] (const std::vector<Database::IdType>& trackIds)
	{
		playqueue->addTracks(trackIds);
	});

	explore->tracksPlay.connect([=] (const std::vector<Database::IdType>& trackIds)
	{
		playqueue->playTracks(trackIds);
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

	playqueue->trackSelected.connect([=] (Database::IdType trackId, bool play)
	{
		_events.lastLoadedTrackId = trackId;
		_events.trackLoaded(trackId, play);
	});

	playqueue->trackUnselected.connect([=]
	{
		_events.lastLoadedTrackId.reset();
		_events.trackUnloaded();
	});

	// Events from MediaScanner
	{
		const std::string sessionId {LmsApp->sessionId()};
		ServiceProvider<Scanner::MediaScanner>::get()->scanComplete().connect(this, [=] ()
		{
			Wt::WServer::instance()->post(sessionId, [=]
			{
				_events.dbScanned.emit();
				triggerUpdate();
			});
		});

		ServiceProvider<Scanner::MediaScanner>::get()->scanInProgress().connect(this, [=] (Scanner::ScanProgressStats stats)
		{
			Wt::WServer::instance()->post(sessionId, [=]
			{
				_events.dbScanInProgress.emit(stats);
				triggerUpdate();
			});
		});

		ServiceProvider<Scanner::MediaScanner>::get()->scheduled().connect(this, [=] (Wt::WDateTime dateTime)
		{
			Wt::WServer::instance()->post(sessionId, [=]
			{
				_events.dbScanScheduled.emit(dateTime);
				triggerUpdate();
			});
		});

	}

	_events.dbScanned.connect([=] ()
	{
		if (isUserAdmin())
		{
			const auto& stats {*ServiceProvider<Scanner::MediaScanner>::get()->getStatus().lastCompleteScanStats};

			notifyMsg(MsgType::Info, Wt::WString::tr("Lms.Admin.Database.scan-complete")
				.arg(static_cast<unsigned>(stats.nbFiles()))
				.arg(static_cast<unsigned>(stats.additions))
				.arg(static_cast<unsigned>(stats.updates))
				.arg(static_cast<unsigned>(stats.deletions))
				.arg(static_cast<unsigned>(stats.duplicates.size()))
				.arg(static_cast<unsigned>(stats.errors.size())));
		}
	});

	// Events from Application group
	_events.appOpen.connect([=] (LmsApplicationInfo)
	{
		// Only one active session by user
		if (!LmsApp->isUserDemo())
		{
			setConfirmCloseMessage("");
			quit(Wt::WString::tr("Lms.quit-other-session"));
		}
	});

	internalPathChanged().connect([=]
	{
		handlePathChange(mainStack, isUserAdmin());
	});

	handlePathChange(mainStack, isUserAdmin());
}

void
LmsApplication::notify(const Wt::WEvent& event)
{
	try
	{
		WApplication::notify(event);
	}
	catch (LmsApplicationException& e)
	{
		LMS_LOG(UI, WARNING) << "Caught a LmsApplication exception: " << e.what();
		handleException(e);
	}
	catch (std::exception& e)
	{
		LMS_LOG(UI, ERROR) << "Caught exception: " << e.what();
		throw LmsException {"Internal error"}; // Do not put details here at it may appear on the user rendered html
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
	Wt::WServer::instance()->post(LmsApp->sessionId(), std::move(func));
}

static
std::string
escape(const std::string& str)
{
	return replaceInString(std::move(str), "\'", "\\\'");
}

void
LmsApplication::notifyMsg(MsgType type, const Wt::WString& message, std::chrono::milliseconds duration)
{
	LMS_LOG(UI, INFO) << "Notifying message '" << message.toUTF8() << "' of type '" << msgTypeToString(type) << "'";

	std::ostringstream oss;

	oss << "$.notify({"
			"message: '" << escape(message.toUTF8()) << "'"
		"},{"
			"type: '" << msgTypeToString(type) << "',"
			"placement: {from: 'top', align: 'center'},"
			"timer: 250,"
			"delay: " << duration.count() << ""
		"});";

	LmsApp->doJavaScript(oss.str());
}


} // namespace UserInterface


