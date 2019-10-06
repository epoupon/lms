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

#include "scanner/MediaScanner.hpp"

#include "LmsApplicationGroup.hpp"

namespace Database {
	class Artist;
	class Cluster;
	class Db;
	class Release;
	class User;
}

namespace UserInterface {

class AudioResource;
class Auth;
class ImageResource;
class LmsApplicationException;

// Events that can be listen from anywhere in the application
struct Events
{
	// Events relative to group
	Wt::Signal<LmsApplicationInfo> appOpen;
	Wt::Signal<LmsApplicationInfo> appClosed;

	// A track is being loaded
	Wt::Signal<Database::IdType /* trackId */, bool /* play */> trackLoaded;
	std::optional<Database::IdType> lastLoadedTrackId;
	// Unload current track
	Wt::Signal<> trackUnloaded;

	// Database events
	Wt::Signal<Scanner::MediaScanner::Stats> dbScanned;
	Wt::Signal<Scanner::MediaScanner::Stats> dbScanInProgress;
	Wt::Signal<Wt::WDateTime> dbScanScheduled;
};

// Used to classify the message sent to the user
enum class MsgType
{
	Success,
	Info,
	Warning,
	Danger,
};

class LmsApplication : public Wt::WApplication
{
	public:
		LmsApplication(const Wt::WEnvironment& env, std::unique_ptr<Database::Session> dbSession, LmsApplicationGroupContainer& appGroups);

		static std::unique_ptr<Wt::WApplication> create(const Wt::WEnvironment& env, Database::Db& db, LmsApplicationGroupContainer& appGroups);
		static LmsApplication* instance();

		// Session application data
		std::shared_ptr<ImageResource> getImageResource() { return _imageResource; }
		std::shared_ptr<AudioResource> getAudioResource() { return _audioResource; }
		Database::Session& getDbSession() { return *_dbSession.get();}

		Wt::Dbo::ptr<Database::User> getUser() const;
		bool isUserAuthStrong() const; // user must be logged in prior this call
		bool isUserAdmin() const; // user must be logged in prior this call
		bool isUserDemo() const; // user must be logged in prior this call
		std::string getUserLoginName() const; // user must be logged in prior this call

		Events& getEvents() { return _events; }

		// Utils
		void post(std::function<void()> func);
		void notifyMsg(MsgType type, const Wt::WString& message, std::chrono::milliseconds duration = std::chrono::milliseconds(4000));

		static Wt::WLink createArtistLink(Wt::Dbo::ptr<Database::Artist> artist);
		static std::unique_ptr<Wt::WAnchor> createArtistAnchor(Wt::Dbo::ptr<Database::Artist> artist, bool addText = true);
		static Wt::WLink createReleaseLink(Wt::Dbo::ptr<Database::Release> release);
		static std::unique_ptr<Wt::WAnchor> createReleaseAnchor(Wt::Dbo::ptr<Database::Release> release, bool addText = true);
		static std::unique_ptr<Wt::WTemplate> createCluster(Wt::Dbo::ptr<Database::Cluster> cluster, bool canDelete = false);

		// Signal emitted just before the session ends (user may already be logged out)
		Wt::Signal<>&	preQuit() { return _preQuit; }

	private:

		void handleException(LmsApplicationException& e);
		void goHomeAndQuit();

		LmsApplicationGroup& getApplicationGroup();

		// Signal slots
		void handleUserLoggedOut();
		void handleUserLoggedIn(Database::IdType userId, bool strongAuth);

		void notify(const Wt::WEvent& event) override;
		void finalize() override;

		void createHome();

		Wt::Signal<>				_preQuit;
		std::unique_ptr<Database::Session>	_dbSession;
		LmsApplicationGroupContainer&   	_appGroups;
		Events					_events;
		std::optional<Database::IdType>	_userId;
		std::optional<bool>			_userAuthStrong;
		std::shared_ptr<ImageResource>		_imageResource;
		std::shared_ptr<AudioResource>		_audioResource;
};


// Helper to get session instance
#define LmsApp	LmsApplication::instance()

} // namespace UserInterface

