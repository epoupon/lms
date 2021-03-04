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

#include "scanner/ScannerEvents.hpp"

#include "LmsApplicationGroup.hpp"

namespace Database
{
	class Artist;
	class Cluster;
	class Db;
	class Release;
	class Session;
	class User;
}
namespace Wt
{
	class WPopupMenu;
}

namespace UserInterface {

class CoverResource;
class LmsApplicationException;
class MediaPlayer;
class PlayQueue;

// Events that can be listen from anywhere in the application
struct Events
{
	// Events relative to group
	Wt::Signal<LmsApplicationInfo> appOpen;
	Wt::Signal<LmsApplicationInfo> appClosed;
};

class LmsApplication : public Wt::WApplication
{
	public:

		LmsApplication(const Wt::WEnvironment& env, Database::Db& db, LmsApplicationGroupContainer& appGroups);
		~LmsApplication();

		static std::unique_ptr<Wt::WApplication> create(const Wt::WEnvironment& env, Database::Db& db, LmsApplicationGroupContainer& appGroups);
		static LmsApplication* instance();


		// Session application data
		std::shared_ptr<CoverResource> getCoverResource() { return _coverResource; }
		Database::Session& getDbSession(); // always thread safe

		Wt::Dbo::ptr<Database::User> getUser();
		bool isUserAuthStrong() const; // user must be logged in prior this call
		bool isUserAdmin(); // user must be logged in prior this call
		bool isUserDemo(); // user must be logged in prior this call
		std::string getUserLoginName(); // user must be logged in prior this call

		Events& getEvents() { return _events; }
		Scanner::Events& getScannerEvents() { return _scannerEvents; }

		// Utils
		void post(std::function<void()> func);

		// Used to classify the message sent to the user
		enum class MsgType
		{
			Success,
			Info,
			Warning,
			Danger,
		};
		void notifyMsg(MsgType type, const Wt::WString& message, std::chrono::milliseconds duration = std::chrono::milliseconds {4000});

		static Wt::WLink createArtistLink(Wt::Dbo::ptr<Database::Artist> artist);
		static std::unique_ptr<Wt::WAnchor> createArtistAnchor(Wt::Dbo::ptr<Database::Artist> artist, bool addText = true);
		static Wt::WLink createReleaseLink(Wt::Dbo::ptr<Database::Release> release);
		static std::unique_ptr<Wt::WAnchor> createReleaseAnchor(Wt::Dbo::ptr<Database::Release> release, bool addText = true);
		static std::unique_ptr<Wt::WText> createCluster(Wt::Dbo::ptr<Database::Cluster> cluster, bool canDelete = false);
		Wt::WPopupMenu* createPopupMenu();

		MediaPlayer&	getMediaPlayer() const { return *_mediaPlayer; }
		PlayQueue&		getPlayQueue() const { return *_playQueue; }

		// Signal emitted just before the session ends (user may already be logged out)
		Wt::Signal<>&	preQuit() { return _preQuit; }

	private:

		void init();
		void setTheme();
		void processEnvAuth();
		void processPasswordAuth();
		void handleException(LmsApplicationException& e);
		void goHomeAndQuit();

		LmsApplicationGroup& getApplicationGroup();

		// Signal slots
		void logoutUser();
		void onUserLoggedIn();

		void notify(const Wt::WEvent& event) override;
		void finalize() override;

		void createHome();

		Database::Db&							_db;
		Wt::Signal<>							_preQuit;
		LmsApplicationGroupContainer&   		_appGroups;
		Events									_events;
		Scanner::Events							_scannerEvents;
		struct UserAuthInfo
		{
			Database::IdType	userId;
			bool				strongAuth {};
		};
		std::optional<UserAuthInfo>				_authenticatedUser;
		std::shared_ptr<CoverResource>			_coverResource;
		MediaPlayer*							_mediaPlayer {};
		PlayQueue*								_playQueue {};
		std::unique_ptr<Wt::WPopupMenu>			_popupMenu;
};


// Helper to get session instance
#define LmsApp	::UserInterface::LmsApplication::instance()

} // namespace UserInterface

