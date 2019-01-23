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

#ifndef LMS_APPLICATION_HPP
#define LMS_APPLICATION_HPP

#include <boost/optional.hpp>

#include <Wt/WApplication.h>
#include <Wt/Dbo/SqlConnectionPool.h>

#include "database/DatabaseHandler.hpp"
#include "scanner/MediaScanner.hpp"

#include "LmsApplicationGroup.hpp"
#include "Auth.hpp"

namespace Database {
	class Artist;
	class Cluster;
	class Release;
}

namespace UserInterface {

class AudioResource;
class ImageResource;

// Events that can be listen from anywhere in the application
struct Events
{
	// Events relative to group
	Wt::Signal<LmsApplicationInfo> appOpen;
	Wt::Signal<LmsApplicationInfo> appClosed;

	// A track is being loaded
	Wt::Signal<Database::IdType /* trackId */, bool /* play */> trackLoaded;
	boost::optional<Database::IdType> lastLoadedTrackId;
	// Unload current track
	Wt::Signal<> trackUnloaded;

	// A database scan is complete
	Wt::Signal<Scanner::MediaScanner::Stats> dbScanned;
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
		LmsApplication(const Wt::WEnvironment& env, Wt::Dbo::SqlConnectionPool& connectionPool, LmsApplicationGroupContainer& appGroups);

		static std::unique_ptr<Wt::WApplication> create(const Wt::WEnvironment& env,
				Wt::Dbo::SqlConnectionPool& connectionPool, LmsApplicationGroupContainer& appGroups);
		static LmsApplication* instance();

		// Session application data
		std::shared_ptr<ImageResource> getImageResource() { return _imageResource; }
		std::shared_ptr<AudioResource> getAudioResource() { return _audioResource; }
		Database::Handler& getDb() { return _db;}
		Wt::Dbo::Session& getDboSession() { return _db.getSession();}

		const Wt::Auth::User& getAuthUser() { return _db.getLogin().user(); }
		Database::User::pointer getUser() { return _db.getCurrentUser(); }
		Wt::WString getUserIdentity() { return _userIdentity; }

		Events& getEvents() { return _events; }

		// Utils
		void goHome();
		void goHomeAndQuit();

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

		LmsApplicationGroup& getApplicationGroup();

		// Events
		void handleAuthEvent();
		void notify(const Wt::WEvent& event) override;
		void finalize() override;

		void createHome();

		Wt::Signal<>		_preQuit;
		Database::Handler	_db;
		LmsApplicationGroupContainer&   _appGroups;
		Events			_events;
		Wt::WString		_userIdentity;
		Auth*			_auth = nullptr;
		std::shared_ptr<ImageResource>	_imageResource;
		std::shared_ptr<AudioResource>	_audioResource;
		bool			_isAdmin = false;
};


// Helper to get session instance
#define LmsApp	LmsApplication::instance()

} // namespace UserInterface

#endif

