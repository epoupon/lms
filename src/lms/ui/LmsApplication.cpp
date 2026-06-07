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

#include <Wt/WConfig.h>

#include <Wt/WAnchor.h>
#include <Wt/WEnvironment.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPushButton.h>
#include <Wt/WServer.h>
#include <Wt/WStackedWidget.h>

#include "core/IConfig.hpp"
#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"
#include "core/Service.hpp"

#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/Artist.hpp"
#include "database/objects/Cluster.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/TrackList.hpp"
#include "database/objects/User.hpp"
#include "database/profiling/IQueryProfiler.hpp"
#include "services/artwork/IArtworkService.hpp"
#include "services/auth/IAuthTokenService.hpp"
#include "services/auth/IEnvService.hpp"
#include "services/auth/IPasswordService.hpp"
#include "services/scrobbling/IScrobblingService.hpp"

#include "Auth.hpp"
#include "LmsApplicationException.hpp"
#include "LmsApplicationManager.hpp"
#include "LmsTheme.hpp"
#include "MediaPlayer.hpp"
#include "ModalManager.hpp"
#include "NotificationContainer.hpp"
#include "PlayQueue.hpp"
#include "admin/About.hpp"
#include "admin/AdminView.hpp"
#include "admin/InitWizardView.hpp"
#include "common/PathRouter.hpp"
#include "common/Template.hpp"
#include "explore/Explore.hpp"
#include "explore/Filters.hpp"
#include "resource/ArtworkResource.hpp"
#include "resource/AudioTranscodingResource.hpp"
#include "settings/SettingsView.hpp"

namespace lms::ui
{
    namespace
    {
        constexpr const char* defaultPath{ "/releases" };

        std::shared_ptr<Wt::WMessageResourceBundle> createMessageResourceBundle()
        {
            const std::string appRoot{ Wt::WApplication::appRoot() };

            auto res{ std::make_shared<Wt::WMessageResourceBundle>() };
            res->use(appRoot + "admin-about");
            res->use(appRoot + "admin-db");
            res->use(appRoot + "admin-debugtools");
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
            res->use(appRoot + "notifications");
            res->use(appRoot + "playqueue");
            res->use(appRoot + "release");
            res->use(appRoot + "releases");
            res->use(appRoot + "settings-audio");
            res->use(appRoot + "settings-password");
            res->use(appRoot + "settings-services");
            res->use(appRoot + "settings-subsonic");
            res->use(appRoot + "settings-ui");
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

        Wt::WLocale createLocale(const std::string& name, [[maybe_unused]] const std::string& timeZoneName)
        {
            Wt::WLocale locale{ name };
            locale.setDecimalPoint(Wt::WString::tr("Lms.locale.decimal-point").toUTF8());
            locale.setGroupSeparator(Wt::WString::tr("Lms.locale.group-separator").toUTF8());
            locale.setDateFormat(Wt::WString::tr("Lms.locale.date-format").toUTF8());
            locale.setTimeFormat(Wt::WString::tr("Lms.locale.time-format").toUTF8());
            locale.setDateTimeFormat(Wt::WString::tr("Lms.locale.date-time-format").toUTF8());

#ifdef WT_DATE_TZ_USE_STD
            if (!timeZoneName.empty())
            {
                try
                {
                    locale.setTimeZone(std::chrono::locate_zone(timeZoneName));
                }
                catch (const std::runtime_error&)
                {
                    // unknown zone, display stays UTC
                }
            }
#endif
            return locale;
        }

    } // namespace

    std::unique_ptr<Wt::WApplication> LmsApplication::create(const Wt::WEnvironment& env, db::IDb& db, LmsApplicationManager& appManager, AuthenticationBackend authBackend)
    {
        return std::make_unique<LmsApplication>(env, db, appManager, authBackend);
    }

    LmsApplication* LmsApplication::instance()
    {
        return static_cast<LmsApplication*>(Wt::WApplication::instance());
    }

    LmsApplication::LmsApplication(const Wt::WEnvironment& env, db::IDb& db, LmsApplicationManager& appManager, AuthenticationBackend authBackend)
        : Wt::WApplication{ env }
        , _db{ db }
        , _appManager{ appManager }
        , _authBackend{ authBackend }
        , _areDownloadsEnabled(core::Service<core::IConfig>::get()->getBool("ui-allow-downloads", true))
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

    db::IDb& LmsApplication::getDb()
    {
        return _db;
    }

    db::Session& LmsApplication::getDbSession()
    {
        return _db.getTLSSession();
    }

    db::User::pointer LmsApplication::getUser()
    {
        if (!_user)
            return {};

        return db::User::find(getDbSession(), _user->userId);
    }

    db::UserId LmsApplication::getUserId() const
    {
        assert(_user);
        return _user->userId;
    }

    bool LmsApplication::isUserAuthStrong() const
    {
        assert(_user);
        return _user->strongAuth;
    }

    db::UserType LmsApplication::getUserType() const
    {
        assert(_user);
        return _user->userType;
    }

    std::string_view LmsApplication::getUserLoginName() const
    {
        assert(_user);
        return _user->userLoginName;
    }

    bool LmsApplication::areDownloadsEnabled() const
    {
        return _areDownloadsEnabled;
    }

    void LmsApplication::init()
    {
        LMS_SCOPED_TRACE_OVERVIEW("UI", "ApplicationInit");

        setTheme(std::make_shared<LmsTheme>());

        useStyleSheet("resources/font-awesome/css/font-awesome.min.css");
        require("js/mediaplayer.js");

        setTitle();
        setLocalizedStrings(getOrCreateMessageBundle());
        setLocale(createLocale(Wt::WLocale::currentLocale().name(), environment().timeZoneName()));

        // Handle Media Scanner events and other session events
        enableUpdates(true);

        db::UserId userId;
        switch (_authBackend)
        {
        case AuthenticationBackend::Env:
            {
                const auto checkResult{ core::Service<auth::IEnvService>::get()->processEnv(environment()) };
                if (checkResult.state != auth::IEnvService::CheckResult::State::Granted)
                {
                    LMS_LOG(UI, ERROR, "Cannot authenticate user from environment!");
                    throw core::LmsException{ "Cannot authenticate user from environment!" }; // Do not put details here at it may appear on the user rendered html
                }
                assert(checkResult.userId.isValid());
                userId = checkResult.userId;
            }
            break;

        case AuthenticationBackend::Internal:
            [[fallthrough]];
        case AuthenticationBackend::PAM:
            // Try to authenticate using auth token ("remember me" checkbox), may fail
            userId = processAuthToken(environment());
            break;
        }

        if (userId.isValid())
            onUserLoggedIn(userId, false /* strongAuth */);
        else
            processPasswordAuth();
    }

    void LmsApplication::processPasswordAuth()
    {
        // If here is no account in the database, launch the first connection wizard
        bool firstConnection{};
        {
            auto transaction{ getDbSession().createReadTransaction() };
            firstConnection = db::User::getCount(getDbSession()) == 0;
        }

        LMS_LOG(UI, DEBUG, "Creating root widget. First connection = " << firstConnection);

        assert(_authBackend == AuthenticationBackend::Internal || _authBackend == AuthenticationBackend::PAM);
        auth::IPasswordService& passwordService{ *core::Service<auth::IPasswordService>::get() };

        if (firstConnection && _authBackend == AuthenticationBackend::Internal)
        {
            root()->addNew<InitWizardView>(passwordService);
            return;
        }

        PasswordAuth* auth{ root()->addNew<PasswordAuth>(passwordService) };
        auth->userLoggedIn.connect(this, [this](db::UserId userId) {
            onUserLoggedIn(userId, true /* strongAuth */);
        });
    }

    void LmsApplication::finalize()
    {
        if (_user)
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
        btn->clicked().connect([this]() {
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
        core::Service<auth::IAuthTokenService>::get()->clearAuthTokens("ui", getUserId());
        LMS_LOG(UI, INFO, "User '" << getUserLoginName() << " 'logged out");
        goHomeAndQuit();
    }

    void LmsApplication::onUserLoggedIn(db::UserId userId, bool strongAuth)
    {
        root()->clear();

        setUserInfo(userId, strongAuth);

        LMS_LOG(UI, INFO, "User '" << getUserLoginName() << "' logged in from '" << environment().clientAddress() << "', user agent = " << environment().userAgent() << ", locale = '" << locale().name() << "', timezone = '" << environment().timeZoneName() << "'");

        _appManager.registerApplication(*this);
        _appManager.applicationRegistered.connect(this, [this](LmsApplication& otherApplication) {
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

    void LmsApplication::setUserInfo(db::UserId userId, bool strongAuth)
    {
        auto transaction{ getDbSession().createReadTransaction() };

        const db::User::pointer user{ db::User::find(getDbSession(), userId) };
        if (!user)
            throw core::LmsException{ "Internal error" }; // Do not put details here at it may appear on the user rendered html

        assert(!_user);
        _user = UserAuthInfo{
            .userId = userId,
            .userType = user->getType(),
            .userLoginName = user->getLoginName(),
            .strongAuth = strongAuth
        };
    }

    void LmsApplication::createHome()
    {
        LMS_SCOPED_TRACE_OVERVIEW("UI", "ApplicationCreateHome");

        _artworkResource = std::make_shared<ArtworkResource>();

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
        navbar->bindString("username", std::string{ getUserLoginName() }, Wt::TextFormat::Plain);

        navbar->bindNew<Wt::WAnchor>("settings-ui", Wt::WLink{ Wt::LinkType::InternalPath, "/settings/ui" }, Wt::WString::tr("Lms.Settings.menu-ui"));
        navbar->bindNew<Wt::WAnchor>("settings-audio", Wt::WLink{ Wt::LinkType::InternalPath, "/settings/audio" }, Wt::WString::tr("Lms.Settings.menu-audio"));
        if (core::Service<core::IConfig>::get()->getBool("api-subsonic", true))
        {
            navbar->setCondition("if-has-subsonic-api-menu", true);
            navbar->bindNew<Wt::WAnchor>("settings-subsonic", Wt::WLink{ Wt::LinkType::InternalPath, "/settings/subsonic" }, Wt::WString::tr("Lms.Settings.menu-subsonic"));
        }
        navbar->bindNew<Wt::WAnchor>("settings-services", Wt::WLink{ Wt::LinkType::InternalPath, "/settings/services" }, Wt::WString::tr("Lms.Settings.menu-services"));

        if (getAuthBackend() == AuthenticationBackend::Internal)
        {
            navbar->setCondition("if-has-change-password-menu", true);
            navbar->bindNew<Wt::WAnchor>("settings-password", Wt::WLink{ Wt::LinkType::InternalPath, "/settings/password" }, Wt::WString::tr("Lms.Settings.menu-password"));
        }

        {
            Wt::WAnchor* logout{ navbar->bindNew<Wt::WAnchor>("logout") };
            logout->setText(Wt::WString::tr("Lms.logout"));
            logout->clicked().connect(this, &LmsApplication::logoutUser);
        }

        if (LmsApp->getUserType() == db::UserType::ADMIN)
        {
            navbar->setCondition("if-is-admin", true);
            auto* about{ navbar->bindNew<Wt::WPushButton>("about", Wt::WString::tr("Lms.Admin.menu-about"), Wt::TextFormat::XHTML) };
            about->clicked().connect([] {
                showAboutModal();
            });
            navbar->bindNew<Wt::WAnchor>("media-libraries", Wt::WLink{ Wt::LinkType::InternalPath, "/admin/libraries" }, Wt::WString::tr("Lms.Admin.menu-media-libraries"));
            navbar->bindNew<Wt::WAnchor>("scan-settings", Wt::WLink{ Wt::LinkType::InternalPath, "/admin/scan-settings" }, Wt::WString::tr("Lms.Admin.menu-scan-settings"));
            navbar->bindNew<Wt::WAnchor>("scanner", Wt::WLink{ Wt::LinkType::InternalPath, "/admin/scanner" }, Wt::WString::tr("Lms.Admin.menu-scanner"));
            navbar->bindNew<Wt::WAnchor>("users", Wt::WLink{ Wt::LinkType::InternalPath, "/admin/users" }, Wt::WString::tr("Lms.Admin.menu-users"));
            // Hide the entry if no debug service is enabled
            if (core::Service<core::tracing::ITraceLogger>::get()
                || core::Service<db::IQueryProfiler>::get())
            {
                navbar->setCondition("if-debug-tools", true);
                navbar->bindNew<Wt::WAnchor>("debug-tools", Wt::WLink{ Wt::LinkType::InternalPath, "/admin/debug-tools" }, Wt::WString::tr("Lms.Admin.menu-debug-tools"));
            }
        }

        PathRouter* mainRouter{ main->bindNew<PathRouter>("contents") };

        _playQueue = mainRouter->add<PlayQueue>("/playqueue", Wt::WString::tr("Lms.PlayQueue.playqueue"));

        Explore* explore{ mainRouter->add<Explore>("/artists", Wt::WString::tr("Lms.Explore.artists"), *filters, *_playQueue) };
        mainRouter->addRoute("/artist", std::nullopt, explore);
        mainRouter->addRoute("/releases", Wt::WString::tr("Lms.Explore.releases"), explore);
        mainRouter->addRoute("/release", std::nullopt, explore);
        mainRouter->addRoute("/tracks", Wt::WString::tr("Lms.Explore.tracks"), explore);
        mainRouter->addRoute("/tracklists", Wt::WString::tr("Lms.Explore.tracklists"), explore);
        mainRouter->addRoute("/tracklist", std::nullopt, explore);

        mainRouter->add<SettingsView>("/settings", std::nullopt);

        if (getUserType() == db::UserType::ADMIN)
            mainRouter->add<AdminView>("/admin", std::nullopt);

        explore->getPlayQueueController().setMaxTrackCountToEnqueue(_playQueue->getCapacity());

        // Events from MediaPlayer
        _mediaPlayer->playNext.connect([this] {
            LMS_LOG(UI, DEBUG, "Received playNext from player");
            _playQueue->playNext();
        });
        _mediaPlayer->playPrevious.connect([this] {
            LMS_LOG(UI, DEBUG, "Received playPrevious from player");
            _playQueue->playPrevious();
        });

        _mediaPlayer->scrobbleListenNow.connect([this](db::TrackId trackId) {
            LMS_LOG(UI, DEBUG, "Received ScrobbleListenNow from player for trackId = " << trackId.toString());
            const scrobbling::Listen listen{ getUserId(), trackId };
            core::Service<scrobbling::IScrobblingService>::get()->listenStarted(listen);
        });
        _mediaPlayer->scrobbleListenFinished.connect([this](db::TrackId trackId, unsigned durationMs) {
            LMS_LOG(UI, DEBUG, "Received ScrobbleListenFinished from player for trackId = " << trackId.toString() << ", duration = " << (durationMs / 1000) << "s");
            const std::chrono::milliseconds duration{ durationMs };
            const scrobbling::Listen listen{ getUserId(), trackId };
            core::Service<scrobbling::IScrobblingService>::get()->listenFinished(listen, std::chrono::duration_cast<std::chrono::seconds>(duration));
        });

        _mediaPlayer->playbackEnded.connect([this] {
            LMS_LOG(UI, DEBUG, "Received playbackEnded from player");
            _playQueue->onPlaybackEnded();
        });

        _playQueue->trackSelected.connect([this](db::TrackId trackId, bool play, float replayGain) {
            _mediaPlayer->loadTrack(trackId, play, replayGain);
        });

        _playQueue->trackUnselected.connect([this] {
            _mediaPlayer->stop();
        });
        _playQueue->trackCountChanged.connect([this](std::size_t trackCount) {
            _mediaPlayer->onPlayQueueUpdated(trackCount);
        });
        _mediaPlayer->onPlayQueueUpdated(_playQueue->getCount());

        const bool isAdmin{ getUserType() == db::UserType::ADMIN };
        if (isAdmin)
        {
            _scannerEvents.scanComplete.connect([this](const scanner::ScanStats& stats) {
                notifyMsg(Notification::Type::Info,
                          Wt::WString::tr("Lms.Admin.Database.scan-complete")
                              .arg(static_cast<unsigned>(stats.getTotalFileCount()))
                              .arg(static_cast<unsigned>(stats.additions))
                              .arg(static_cast<unsigned>(stats.updates))
                              .arg(static_cast<unsigned>(stats.deletions))
                              .arg(static_cast<unsigned>(stats.duplicates.size()))
                              .arg(static_cast<unsigned>(stats.errorsCount)));
            });
        }

        internalPathChanged().connect([this] {
            LMS_LOG(UI, DEBUG, "Internal path changed to '" << wApp->internalPath() << "'");
            doJavaScript(javaScriptClass() + ".updateActiveNav('" + wApp->internalPath() + "')");
        });

        mainRouter->noMatch().connect([] {
            wApp->setInternalPath(defaultPath, true);
        });

        mainRouter->activate();
        doJavaScript(javaScriptClass() + ".updateActiveNav('" + wApp->internalPath() + "')");
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

    void LmsApplication::notifyMsg(Notification::Type type, const Wt::WString& message, std::chrono::milliseconds duration)
    {
        LMS_LOG(UI, INFO, "Notifying message '" << message.toUTF8() << "'");
        _notificationContainer->add(type, message, duration);
    }
} // namespace lms::ui