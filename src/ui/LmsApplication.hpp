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

#include <boost/filesystem.hpp>
#include <Wt/WApplication>

#include <Wt/Dbo/SqlConnectionPool>

#include "database/DatabaseHandler.hpp"
#include "resource/CoverResource.hpp"
#include "resource/TranscodeResource.hpp"

namespace UserInterface {

class LmsApplication : public Wt::WApplication
{
	public:
		static Wt::WApplication *create(const Wt::WEnvironment& env, Wt::Dbo::SqlConnectionPool& connectionPool);
		static LmsApplication* instance();

		LmsApplication(const Wt::WEnvironment& env, Wt::Dbo::SqlConnectionPool& connectionPool);

		// Session application data
		CoverResource* getCoverResource() { return _coverResource; }
		TranscodeResource* getTranscodeResource() { return _transcodeResource; }
		Database::Handler& getDbHandler() { return _db;}

	private:

		void handleAuthEvent(void);
		void createFirstConnectionUI();
		void createLmsUI();

		Database::Handler	_db;
		CoverResource*          _coverResource;
		TranscodeResource*	_transcodeResource;
};

// Helpers to get session data
#define LmsApp	LmsApplication::instance()

Database::Handler& DbHandler();
Wt::Dbo::Session& DboSession();

const Wt::Auth::User& CurrentAuthUser();
Database::User::pointer CurrentUser();

CoverResource *SessionCoverResource();
TranscodeResource *SessionTranscodeResource();

} // namespace UserInterface

#endif

