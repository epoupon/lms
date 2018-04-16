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

#include <Wt/WApplication.h>
#include <Wt/Dbo/SqlConnectionPool.h>

#include "database/DatabaseHandler.hpp"
#include "scanner/MediaScanner.hpp"

#include "Auth.hpp"

namespace UserInterface {

class TranscodeResource;
class ImageResource;

class LmsApplication : public Wt::WApplication
{
	public:
		LmsApplication(const Wt::WEnvironment& env, Wt::Dbo::SqlConnectionPool& connectionPool, Scanner::MediaScanner& scanner);

		static std::unique_ptr<Wt::WApplication> create(const Wt::WEnvironment& env,
				Wt::Dbo::SqlConnectionPool& connectionPool, Scanner::MediaScanner& scanner);
		static LmsApplication* instance();

		// Session application data
		std::shared_ptr<ImageResource> getImageResource() { return _imageResource; }
		std::shared_ptr<TranscodeResource> getTranscodeResource() { return _transcodeResource; }
		Database::Handler& getDb() { return _db;}
		Wt::Dbo::Session& getDboSession() { return _db.getSession();}

		const Wt::Auth::User& getCurrentAuthUser() { return _db.getLogin().user(); }
		Database::User::pointer getCurrentUser() { return _db.getCurrentUser(); }

		Scanner::MediaScanner& getMediaScanner() { return _scanner; }

		// Utils
		void goHome();
		void goHomeAndQuit();
		void notifyMsg(const Wt::WString& message);

		static std::unique_ptr<Wt::WAnchor> createArtistAnchor(Database::Artist::pointer artist, bool addText = true);
		static std::unique_ptr<Wt::WAnchor> createReleaseAnchor(Database::Release::pointer release, bool addText = true);

	private:

		void handleAuthEvent(void);
		void notify(const Wt::WEvent& event) override;

		Database::Handler	_db;
		Auth*			_auth;
		Scanner::MediaScanner&	_scanner;
		std::shared_ptr<ImageResource>	_imageResource;
		std::shared_ptr<TranscodeResource>	_transcodeResource;
		bool			_isAdmin = false;
};

// Helper to get session data
#define LmsApp	LmsApplication::instance()

} // namespace UserInterface

#endif

