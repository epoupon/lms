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

#pragma once

#include <optional>

#include <Wt/WApplication.h>

#include "database/Object.hpp"
#include "database/UserId.hpp"
#include "database/Types.hpp"
#include "services/scanner/ScannerEvents.hpp"
#include "admin/ScannerController.hpp"
#include "Notification.hpp"

namespace lms::db
{
    class Db;
    class Session;
    class User;
}

namespace lms::ui
{
    class CoverResource;
    class LmsApplicationException;
    class MediaPlayer;
    class PlayQueue;
    class LmsApplicationManager;
    class NotificationContainer;
    class ModalManager;

    class LmsApplication : public Wt::WApplication
    {
    public:
        LmsApplication(const Wt::WEnvironment& env, db::Db& db, LmsApplicationManager& appManager, std::optional<db::UserId> userId = std::nullopt);
        ~LmsApplication();

        static std::unique_ptr<Wt::WApplication> create(const Wt::WEnvironment& env, db::Db& db, LmsApplicationManager& appManager);
        static LmsApplication* instance();

        // Session application data
        std::shared_ptr<CoverResource> getCoverResource() { return _coverResource; }
        db::Db& getDb();
        db::Session& getDbSession(); // always thread safe

        db::ObjectPtr<db::User>	getUser();
        db::UserId				getUserId();
        bool isUserAuthStrong() const; // user must be logged in prior this call
        db::UserType				getUserType(); // user must be logged in prior this call
        std::string						getUserLoginName(); // user must be logged in prior this call

        // Proxified scanner events
        scanner::Events& getScannerEvents() { return _scannerEvents; }

        // Utils
        void post(std::function<void()> func);
        void setTitle(const Wt::WString& title = "");

        // Used to classify the message sent to the user
        void notifyMsg(Notification::Type type, const Wt::WString& category, const Wt::WString& message, std::chrono::milliseconds duration = std::chrono::milliseconds{ 5000 });

        MediaPlayer& getMediaPlayer() const { return *_mediaPlayer; }
        PlayQueue& getPlayQueue() const { return *_playQueue; }
        ModalManager& getModalManager() const { return *_modalManager; }

        // Signal emitted just before the session ends (user may already be logged out)
        Wt::Signal<>& preQuit() { return _preQuit; }

    private:
        void init();
        void processPasswordAuth();
        void handleException(LmsApplicationException& e);
        void goHomeAndQuit();

        // Signal slots
        void logoutUser();
        void onUserLoggedIn();

        void notify(const Wt::WEvent& event) override;
        void finalize() override;

        void createHome();

        db::Db& _db;
        Wt::Signal<>							_preQuit;
        LmsApplicationManager& _appManager;
        scanner::Events							_scannerEvents;
        struct UserAuthInfo
        {
            db::UserId	userId;
            bool				strongAuth{};
        };
        std::optional<UserAuthInfo>				_authenticatedUser;
        std::shared_ptr<CoverResource>			_coverResource;
        MediaPlayer* _mediaPlayer{};
        PlayQueue* _playQueue{};
        NotificationContainer* _notificationContainer{};
        ModalManager* _modalManager{};
    };


    // Helper to get session instance
#define LmsApp	lms::ui::LmsApplication::instance()

} // namespace lms::ui

