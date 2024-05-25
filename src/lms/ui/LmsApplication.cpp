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
#include <Wt/WEnvironment.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPushButton.h>
#include <Wt/WServer.h>
#include <Wt/WStackedWidget.h>

#include "services/auth/IEnvService.hpp"
#include "services/auth/IPasswordService.hpp"
#include "services/cover/ICoverService.hpp"
#include "database/Artist.hpp"
#include "database/Cluster.hpp"
#include "database/Db.hpp"
#include "database/Release.hpp"
#include "database/Session.hpp"
#include "database/TrackList.hpp"
#include "database/User.hpp"
#include "services/scrobbling/IScrobblingService.hpp"
#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"
#include "core/Service.hpp"
#include "core/String.hpp"

#include "admin/InitWizardView.hpp"
#include "admin/MediaLibrariesView.hpp"
#include "admin/TracingView.hpp"
#include "admin/ScannerController.hpp"
#include "admin/ScanSettingsView.hpp"
#include "admin/UserView.hpp"
#include "admin/UsersView.hpp"
#include "common/Template.hpp"
#include "explore/Explore.hpp"
#include "explore/Filters.hpp"
#include "resource/AudioFileResource.hpp"
#include "resource/AudioTranscodingResource.hpp"
#include "resource/CoverResource.hpp"
#include "Auth.hpp"
#include "LmsApplicationException.hpp"
#include "LmsApplicationManager.hpp"
#include "LmsTheme.hpp"
#include "MediaPlayer.hpp"
#include "ModalManager.hpp"
#include "NotificationContainer.hpp"
#include "PlayQueue.hpp"
#include "SettingsView.hpp"

namespace lms::ui
{
    namespace
    {
        constexpr const char* defaultPath{ "/releases" };

        std::shared_ptr<Wt::WMessageResourceBundle> createMessageResourceBundle()
        {
            const std::string appRoot{ Wt::WApplication::appRoot() };

            auto res{ std::make_shared<Wt::WMessageResourceBundle>() };
            res->use(appRoot + "admin-initwizard");
            res->use(appRoot + "admin-medialibraries");
            res->use(appRoot + "admin-medialibrary");
            res->use(appRoot + "admin-scannercontroller");
            res->use(appRoot + "admin-scansettings");
            res->use(appRoot + "admin-tracing");
            res->use(appRoot + "admin-user");
            res->use(appRoot + "admin-users");
            res->use(appRoot + "artist");
            res->use(appRoot + "artists");
            res->use(appRoot + "error");
            res->use(appRoot + "explore");
            res->use(appRoot + "login");
            res->use(appRoot + "main");
            res->use(appRoot + "mediaplayer");
            res->use(appRoot + "messages");
            res->use(appRoot + "misc");
            res->use(appRoot + "multisearch");
            res->use(appRoot + "notifications");
            res->use(appRoot + "playqueue");
            res->use(appRoot + "release");
            res->use(appRoot + "releases");
            res->use(appRoot + "settings");
            res->use(appRoot + "tracklist");
            res->use(appRoot + "tracklists");
            res->use(appRoot + "tracks");

            return res;
        }

        std::shared_ptr<Wt::WMessageResourceBundle> getOrCreateMessageBundle()
        {
            static std::shared_ptr<Wt::WMessageResourceBundle> res{ createMessageResourceBundle() };
            return res;
        }

        enum IdxRoot
        {
            IdxExplore = 0,
            IdxPlayQueue,
            IdxSettings,
            IdxAdminLibraries,
            IdxAdminScanSettings,
            IdxAdminScanner,
            IdxAdminUsers,
            IdxAdminUser,
            IdxAdminTracing,
        };

        void handlePathChange(Wt::WStackedWidget& stack, bool isAdmin)
        {
            static const struct
            {
                std::string path;
                int index;
                bool admin;
                std::optional<Wt::WString> title;
            } views[] =
            {
                { "/artists",			    IdxExplore,			    false,	Wt::WString::tr("Lms.Explore.artists") },
                { "/artist",			    IdxExplore,			    false,	std::nullopt },
                { "/releases",			    IdxExplore,			    false,	Wt::WString::tr("Lms.Explore.releases") },
                { "/release",			    IdxExplore,			    false,	std::nullopt },
                { "/tracks",			    IdxExplore,			    false,	Wt::WString::tr("Lms.Explore.tracks") },
                { "/tracklists",		    IdxExplore,			    false,	Wt::WString::tr("Lms.Explore.tracklists") },
                { "/tracklist",			    IdxExplore,			    false,	std::nullopt },
                { "/multisearch",            IdxExplore,               true,     std::nullopt },
                { "/playqueue",			    IdxPlayQueue,		    false,	Wt::WString::tr("Lms.PlayQueue.playqueue") },
                { "/settings",			    IdxSettings,		    false,	Wt::WString::tr("Lms.Settings.settings") },
                { "/admin/libraries",	    IdxAdminLibraries,	    true,	Wt::WString::tr("Lms.Admin.MediaLibraries.media-libraries") },
                { "/admin/scan-settings",	IdxAdminScanSettings,	true,	Wt::WString::tr("Lms.Admin.Database.scan-settings") },
                { "/admin/scanner",	        IdxAdminScanner,	    true,	Wt::WString::tr("Lms.Admin.ScannerController.scanner") },
                { "/admin/users",		    IdxAdminUsers,		    true,	Wt::WString::tr("Lms.Admin.Users.users") },
                { "/admin/user",		    IdxAdminUser,		    true,	std::nullopt },
                { "/admin/tracing",		    IdxAdminTracing,		true,	Wt::WString::tr("Lms.Admin.Tracing.tracing") },
            };

            LMS_LOG(UI, DEBUG, "Internal path changed to '" << wApp->internalPath() << "'");

            for (const auto& view : views)
            {
                if (wApp->internalPathMatches(view.path))
                {
                    if (view.admin && !isAdmin)
                        break;

                    stack.setCurrentIndex(view.index);
                    if (view.title)
                        LmsApp->setTitle(*view.title);

                    LmsApp->doJavaScript(LmsApp->javaScriptClass() + ".updateActiveNav('" + view.path + "')");
                    return;
                }
            }

            wApp->setInternalPath(defaultPath, true);
        }
    }

    std::unique_ptr<Wt::WApplication> LmsApplication::create(const Wt::WEnvironment& env, db::Db& db, LmsApplicationManager& appManager)
    {
        if (auto * authEnvService{ core::Service<auth::IEnvService>::get() })
        {
            const auto checkResult{ authEnvService->processEnv(env) };
            if (checkResult.state != auth::IEnvService::CheckResult::State::Granted)
            {
                LMS_LOG(UI, ERROR, "Cannot authenticate user from environment!");
                // return a blank page
                return std::make_unique<Wt::WApplication>(env);
            }

            return std::make_unique<LmsApplication>(env, db, appManager, checkResult.userId);
        }

        return std::make_unique<LmsApplication>(env, db, appManager);
    }

    LmsApplication* LmsApplication::instance()
    {
        return static_cast<LmsApplication*>(Wt::WApplication::instance());
    }

    db::Db& LmsApplication::getDb()
    {
        return _db;
    }

    db::Session& LmsApplication::getDbSession()
    {
        return _db.getTLSSession();
    }

    db::User::pointer LmsApplication::getUser()
    {
        if (!_authenticatedUser)
            return {};

        return db::User::find(getDbSession(), _authenticatedUser->userId);
    }

    db::UserId LmsApplication::getUserId()
    {
        return _authenticatedUser->userId;
    }

    bool LmsApplication::isUserAuthStrong() const
    {
        return _authenticatedUser->strongAuth;
    }

    db::UserType LmsApplication::getUserType()
    {
        auto transaction{ getDbSession().createReadTransaction() };

        return getUser()->getType();
    }

    std::string LmsApplication::getUserLoginName()
    {
        auto transaction{ getDbSession().createReadTransaction() };

        return getUser()->getLoginName();
    }

    LmsApplication::LmsApplication(const Wt::WEnvironment& env,
        db::Db& db,
        LmsApplicationManager& appManager,
        std::optional<db::UserId> userId)
        : Wt::WApplication{ env }
        , _db{ db }
        , _appManager{ appManager }
        , _authenticatedUser{ userId ? std::make_optional<UserAuthInfo>(UserAuthInfo {*userId, false}) : std::nullopt }
    {
        try
        {
            init();
        }
        catch (LmsApplicationException& e)
        {
            LMS_LOG(UI, WARNING, "Caught a LmsApplication exception: " << e.what());
            handleException(e);
        }
        catch (std::exception& e)
        {
            LMS_LOG(UI, ERROR, "Caught exception: " << e.what());
            throw core::LmsException{ "Internal error" }; // Do not put details here at it may appear on the user rendered html
        }
    }

    LmsApplication::~LmsApplication() = default;

    void LmsApplication::init()
    {
        LMS_SCOPED_TRACE_OVERVIEW("UI", "ApplicationInit");

        setTheme(std::make_shared<LmsTheme>());

        useStyleSheet("resources/font-awesome/css/font-awesome.min.css");
        require("js/mediaplayer.js");

        setTitle();
        setLocalizedStrings(getOrCreateMessageBundle());

        // Handle Media Scanner events and other session events
        enableUpdates(true);

        if (_authenticatedUser)
            onUserLoggedIn();
        else if (core::Service<auth::IPasswordService>::exists())
            processPasswordAuth();
    }

    void LmsApplication::processPasswordAuth()
    {
        {
            std::optional<db::UserId> userId{ processAuthToken(environment()) };
            if (userId)
            {
                LMS_LOG(UI, DEBUG, "User authenticated using Auth token!");
                _authenticatedUser = { *userId, false };
                onUserLoggedIn();
                return;
            }
        }

        // If here is no account in the database, launch the first connection wizard
        bool firstConnection{};
        {
            auto transaction{ getDbSession().createReadTransaction() };
            firstConnection = db::User::getCount(getDbSession()) == 0;
        }

        LMS_LOG(UI, DEBUG, "Creating root widget. First connection = " << firstConnection);

        if (firstConnection && core::Service<auth::IPasswordService>::get()->canSetPasswords())
        {
            root()->addWidget(std::make_unique<InitWizardView>());
        }
        else
        {
            Auth* auth{ root()->addNew<Auth>() };
            auth->userLoggedIn.connect(this, [this](db::UserId userId)
                {
                    _authenticatedUser = { userId, true };
                    onUserLoggedIn();
                });
        }
    }

    void LmsApplication::finalize()
    {
        if (_authenticatedUser)
            _appManager.unregisterApplication(*this);

        preQuit().emit();
    }

    void LmsApplication::handleException(LmsApplicationException& e)
    {
        root()->clear();
        Wt::WTemplate* t{ root()->addNew<Wt::WTemplate>(Wt::WString::tr("Lms.Error.template")) };
        t->addFunction("tr", &Wt::WTemplate::Functions::tr);

        t->bindString("error", e.what(), Wt::TextFormat::Plain);
        Wt::WPushButton* btn{ t->bindNew<Wt::WPushButton>("btn-go-home", Wt::WString::tr("Lms.Error.go-home")) };
        btn->clicked().connect([this]()
            {
                redirect(defaultPath);
            });
    }

    void LmsApplication::goHomeAndQuit()
    {
        WApplication::quit("");
        redirect(".");
    }

    void LmsApplication::logoutUser()
    {
        {
            auto transaction{ getDbSession().createWriteTransaction() };
            getUser().modify()->clearAuthTokens();
        }

        LMS_LOG(UI, INFO, "User '" << getUserLoginName() << " 'logged out");
        goHomeAndQuit();
    }

    void LmsApplication::onUserLoggedIn()
    {
        root()->clear();

        LMS_LOG(UI, INFO, "User '" << getUserLoginName() << "' logged in from '" << environment().clientAddress() << "', user agent = " << environment().userAgent());

        _appManager.registerApplication(*this);
        _appManager.applicationRegistered.connect(this, [this](LmsApplication& otherApplication)
            {
                // Only one active session by user
                if (otherApplication.getUserId() == getUserId())
                {
                    if (LmsApp->getUserType() != db::UserType::DEMO)
                    {
                        quit(Wt::WString::tr("Lms.quit-other-session"));
                    }
                }
            });

        createHome();
    }

    void LmsApplication::createHome()
    {
        LMS_SCOPED_TRACE_OVERVIEW("UI", "ApplicationCreateHome");

        _coverResource = std::make_shared<CoverResource>();

        declareJavaScriptFunction("onLoadCover", "function(id) { id.className += \" Lms-cover-loaded\"}");
        declareJavaScriptFunction("updateActiveNav",
            R"(function(current) {
    const menuItems = document.querySelectorAll('.nav-item a[href]:not([href=""])');
    for (const menuItem of menuItems) {
        if (menuItem.getAttribute("href") === current) {
            menuItem.classList.add('active');
        }
        else {
            menuItem.classList.remove('active');
        }

    }
})");

        Wt::WTemplate* main{ root()->addWidget(std::make_unique<Wt::WTemplate>(Wt::WString::tr("Lms.main.template"))) };
        main->addFunction("tr", &Wt::WTemplate::Functions::tr);

        Template* navbar{ main->bindNew<Template>("navbar", Wt::WString::tr("Lms.main.template.navbar")) };
        navbar->addFunction("tr", &Wt::WTemplate::Functions::tr);

        _notificationContainer = main->bindNew<NotificationContainer>("notifications");
        _modalManager = main->bindNew<ModalManager>("modal");

        // MediaPlayer
        _mediaPlayer = main->bindNew<MediaPlayer>("player");

        navbar->bindNew<Wt::WAnchor>("title", Wt::WLink{ Wt::LinkType::InternalPath, defaultPath }, "LMS");
        navbar->bindNew<Wt::WAnchor>("artists", Wt::WLink{ Wt::LinkType::InternalPath, "/artists" }, Wt::WString::tr("Lms.Explore.artists"));
        navbar->bindNew<Wt::WAnchor>("releases", Wt::WLink{ Wt::LinkType::InternalPath, "/releases" }, Wt::WString::tr("Lms.Explore.releases"));
        navbar->bindNew<Wt::WAnchor>("tracks", Wt::WLink{ Wt::LinkType::InternalPath, "/tracks" }, Wt::WString::tr("Lms.Explore.tracks"));
        navbar->bindNew<Wt::WAnchor>("tracklists", Wt::WLink{ Wt::LinkType::InternalPath, "/tracklists" }, Wt::WString::tr("Lms.Explore.tracklists"));

        Filters* filters{ navbar->bindNew<Filters>("filters") };

        navbar->bindString("username", getUserLoginName(), Wt::TextFormat::Plain);
        navbar->bindNew<Wt::WAnchor>("settings", Wt::WLink{ Wt::LinkType::InternalPath, "/settings" }, Wt::WString::tr("Lms.Settings.menu-settings"));

        {
            Wt::WAnchor* logout{ navbar->bindNew<Wt::WAnchor>("logout") };
            logout->setText(Wt::WString::tr("Lms.logout"));
            logout->clicked().connect(this, &LmsApplication::logoutUser);
        }

        if (LmsApp->getUserType() == db::UserType::ADMIN)
        {
            navbar->setCondition("if-is-admin", true);
            navbar->bindNew<Wt::WAnchor>("media-libraries", Wt::WLink{ Wt::LinkType::InternalPath, "/admin/libraries" }, Wt::WString::tr("Lms.Admin.menu-media-libraries"));
            navbar->bindNew<Wt::WAnchor>("scan-settings", Wt::WLink{ Wt::LinkType::InternalPath, "/admin/scan-settings" }, Wt::WString::tr("Lms.Admin.menu-scan-settings"));
            navbar->bindNew<Wt::WAnchor>("scanner", Wt::WLink{ Wt::LinkType::InternalPath, "/admin/scanner" }, Wt::WString::tr("Lms.Admin.menu-scanner"));
            navbar->bindNew<Wt::WAnchor>("users", Wt::WLink{ Wt::LinkType::InternalPath, "/admin/users" }, Wt::WString::tr("Lms.Admin.menu-users"));
            // Hide the entry if the trace logger is not enabled
            if (core::Service<core::tracing::ITraceLogger>::get())
                navbar->bindNew<Wt::WAnchor>("tracing", Wt::WLink{ Wt::LinkType::InternalPath, "/admin/tracing" }, Wt::WString::tr("Lms.Admin.menu-tracing"));
            else
                navbar->bindEmpty("tracing");
        }

        // Contents
        // Order is important in mainStack, see IdxRoot!
        Wt::WStackedWidget* mainStack{ main->bindNew<Wt::WStackedWidget>("contents") };
        mainStack->setOverflow(Wt::Overflow::Visible); // wt makes it hidden by default

        std::unique_ptr<PlayQueue> playQueue{ std::make_unique<PlayQueue>() };

        auto* searEdit = navbar->bindNew<Wt::WLineEdit>("multisearch");

        {
            auto transaction = LmsApp->getDbSession().createReadTransaction();
            if (LmsApp->getUser()->getInterfaceEnableMultisearch())
                navbar->setCondition("if-multisearch-enabled", true);
        }

        Explore* explore{ mainStack->addNew<Explore>(*filters, *playQueue, *searEdit) };
        _playQueue = mainStack->addWidget(std::move(playQueue));
        mainStack->addNew<SettingsView>();

        // Admin stuff
        if (getUserType() == db::UserType::ADMIN)
        {
            mainStack->addNew<MediaLibrariesView>();
            mainStack->addNew<ScanSettingsView>();
            mainStack->addNew<ScannerController>();
            mainStack->addNew<UsersView>();
            mainStack->addNew<UserView>();
            mainStack->addNew<TracingView>();
        }

        explore->getPlayQueueController().setMaxTrackCountToEnqueue(_playQueue->getCapacity());

        // Events from MediaPlayer
        _mediaPlayer->playNext.connect([this]
            {
                LMS_LOG(UI, DEBUG, "Received playNext from player");
                _playQueue->playNext();
            });
        _mediaPlayer->playPrevious.connect([this]
            {
                LMS_LOG(UI, DEBUG, "Received playPrevious from player");
                _playQueue->playPrevious();
            });

        _mediaPlayer->scrobbleListenNow.connect([this](db::TrackId trackId)
            {
                LMS_LOG(UI, DEBUG, "Received ScrobbleListenNow from player for trackId = " << trackId.toString());
                const scrobbling::Listen listen{ getUserId(), trackId };
                core::Service<scrobbling::IScrobblingService>::get()->listenStarted(listen);
            });
        _mediaPlayer->scrobbleListenFinished.connect([this](db::TrackId trackId, unsigned durationMs)
            {
                LMS_LOG(UI, DEBUG, "Received ScrobbleListenFinished from player for trackId = " << trackId.toString() << ", duration = " << (durationMs / 1000) << "s");
                const std::chrono::milliseconds duration{ durationMs };
                const scrobbling::Listen listen{ getUserId(), trackId };
                core::Service<scrobbling::IScrobblingService>::get()->listenFinished(listen, std::chrono::duration_cast<std::chrono::seconds>(duration));
            });

        _mediaPlayer->playbackEnded.connect([this]
            {
                LMS_LOG(UI, DEBUG, "Received playbackEnded from player");
                _playQueue->onPlaybackEnded();
            });

        _playQueue->trackSelected.connect([this](db::TrackId trackId, bool play, float replayGain)
            {
                _mediaPlayer->loadTrack(trackId, play, replayGain);
            });

        _playQueue->trackUnselected.connect([this]
            {
                _mediaPlayer->stop();
            });
        _playQueue->trackCountChanged.connect([this](std::size_t trackCount)
            {
                _mediaPlayer->onPlayQueueUpdated(trackCount);
            });
        _mediaPlayer->onPlayQueueUpdated(_playQueue->getCount());

        const bool isAdmin{ getUserType() == db::UserType::ADMIN };
        if (isAdmin)
        {
            _scannerEvents.scanComplete.connect([this](const scanner::ScanStats& stats)
                {
                    notifyMsg(Notification::Type::Info,
                    Wt::WString::tr("Lms.Admin.Database.database"),
                    Wt::WString::tr("Lms.Admin.Database.scan-complete")
                    .arg(static_cast<unsigned>(stats.nbFiles()))
                        .arg(static_cast<unsigned>(stats.additions))
                        .arg(static_cast<unsigned>(stats.updates))
                        .arg(static_cast<unsigned>(stats.deletions))
                        .arg(static_cast<unsigned>(stats.duplicates.size()))
                        .arg(static_cast<unsigned>(stats.errors.size())));
                });
        }

        internalPathChanged().connect(mainStack, [=]
            {
                handlePathChange(*mainStack, isAdmin);
            });

        handlePathChange(*mainStack, isAdmin);
    }

    void LmsApplication::notify(const Wt::WEvent& event)
    {
        try
        {
            LMS_SCOPED_TRACE_OVERVIEW("UI", "ProcessEvent");
            WApplication::notify(event);
        }
        catch (LmsApplicationException& e)
        {
            LMS_LOG(UI, WARNING, "Caught a LmsApplication exception: " << e.what());
            handleException(e);
        }
        catch (std::exception& e)
        {
            LMS_LOG(UI, ERROR, "Caught exception: " << e.what());
            throw core::LmsException{ "Internal error" }; // Do not put details here at it may appear on the user rendered html
        }
    }

    void LmsApplication::post(std::function<void()> func)
    {
        Wt::WServer::instance()->post(LmsApp->sessionId(), std::move(func));
    }

    void LmsApplication::setTitle(const Wt::WString& title)
    {
        if (title.empty())
            WApplication::setTitle("LMS");
        else
            WApplication::setTitle(title);
    }

    void LmsApplication::notifyMsg(Notification::Type type, const Wt::WString& category, const Wt::WString& message, std::chrono::milliseconds duration)
    {
        LMS_LOG(UI, INFO, "Notifying message '" << message.toUTF8() << "' for category '" << category.toUTF8() << "'");
        _notificationContainer->add(type, category, message, duration);
    }
} // namespace lms::ui