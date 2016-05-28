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

#include <Wt/Dbo/FixedSqlConnectionPool>
#include <Wt/Dbo/backend/Sqlite3>

#include <Wt/Auth/Dbo/AuthInfo>
#include <Wt/Auth/Dbo/UserDatabase>
#include <Wt/Auth/AuthService>
#include <Wt/Auth/HashFunction>
#include <Wt/Auth/PasswordService>
#include <Wt/Auth/PasswordStrengthValidator>
#include <Wt/Auth/PasswordVerifier>

#include "Setting.hpp"

#include "logger/Logger.hpp"

#include "DatabaseHandler.hpp"

namespace Database {


namespace {
	Wt::Auth::AuthService authService;
	Wt::Auth::PasswordService passwordService(authService);
}


void
Handler::configureAuth(void)
{
	authService.setEmailVerificationEnabled(true);

	Wt::Auth::PasswordVerifier *verifier = new Wt::Auth::PasswordVerifier();
	verifier->addHashFunction(new Wt::Auth::BCryptHashFunction(8));
	passwordService.setVerifier(verifier);
	passwordService.setAttemptThrottlingEnabled(true);

	Wt::Auth::PasswordStrengthValidator* strengthValidator = new Wt::Auth::PasswordStrengthValidator();
	// Reduce some constraints...
	strengthValidator->setMinimumLength( Wt::Auth::PasswordStrengthValidator::PassPhrase, 4);
	strengthValidator->setMinimumLength( Wt::Auth::PasswordStrengthValidator::OneCharClass, 4);
	strengthValidator->setMinimumLength( Wt::Auth::PasswordStrengthValidator::TwoCharClass, 4);
	strengthValidator->setMinimumLength( Wt::Auth::PasswordStrengthValidator::ThreeCharClass, 4 );
	strengthValidator->setMinimumLength( Wt::Auth::PasswordStrengthValidator::FourCharClass, 4  );

	passwordService.setStrengthValidator(strengthValidator);
}

const Wt::Auth::AuthService&
Handler::getAuthService()
{
	return authService;
}

const Wt::Auth::PasswordService&
Handler::getPasswordService()
{
	return passwordService;
}


Handler::Handler(Wt::Dbo::SqlConnectionPool& connectionPool)
{
	_session.setConnectionPool(connectionPool);

	_session.mapClass<Database::Artist>("artist");
	_session.mapClass<Database::Cluster>("cluster");
	_session.mapClass<Database::Track>("track");
	_session.mapClass<Database::Playlist>("playlist");
	_session.mapClass<Database::PlaylistEntry>("playlist_entry");
	_session.mapClass<Database::Release>("release");
	_session.mapClass<Database::Video>("video");
	_session.mapClass<Database::MediaDirectory>("media_directory");
	_session.mapClass<Database::Setting>("setting");

	_session.mapClass<Database::User>("user");
	_session.mapClass<Database::AuthInfo>("auth_info");
	_session.mapClass<Database::AuthInfo::AuthIdentityType>("auth_identity");
	_session.mapClass<Database::AuthInfo::AuthTokenType>("auth_token");

	try {
		Wt::Dbo::Transaction transaction(_session);

	        _session.createTables();
	}
	catch(std::exception& e) {
		LMS_LOG(DB, ERROR) << "Cannot create tables: " << e.what();
	}

	{
		Wt::Dbo::Transaction transaction(_session);

		// Indexes
		_session.execute("PRAGMA journal_mode=WAL");
		_session.execute("CREATE INDEX IF NOT EXISTS artist_name_idx ON artist(name)");
		_session.execute("CREATE INDEX IF NOT EXISTS release_name_idx ON release(name)");
		_session.execute("CREATE INDEX IF NOT EXISTS track_artist_idx ON track(artist_id)");
		_session.execute("CREATE INDEX IF NOT EXISTS track_release_idx ON track(release_id)");
		_session.execute("CREATE INDEX IF NOT EXISTS cluster_type_idx ON cluster(type)");

		// Default values
		if (!Setting::exists(_session, "audio_file_extensions"))
			Setting::setString(_session, "audio_file_extensions", ".mp3 .ogg .oga .aac .m4a .flac .wav .wma .aif .aiff .ape .mpc .shn" );

		if (!Setting::exists(_session, "video_file_extensions"))
			Setting::setString(_session, "video_file_extensions", ".flv .avi .mpg .mpeg .mp4 .m4v .mkv .mov .wmv .ogv .divx .m2ts");

		if (!Setting::exists(_session, "tags_highlevel_acousticbrainz"))
			Setting::setBool(_session, "tags_highlevel_acousticbrainz", true);

		if (!Setting::exists(_session, "tags_similarity_acousticbrainz"))
			Setting::setBool(_session,  "tags_similarity_acousticbrain", false);
	}

	_users = new UserDatabase(_session);
}

Handler::~Handler()
{
	delete _users;
}

Wt::Auth::AbstractUserDatabase&
Handler::getUserDatabase()
{
	return *_users;
}

User::pointer
Handler::getCurrentUser()
{
	if (_login.loggedIn())
		return getUser(_login.user());
	else
		return User::pointer();
}

User::pointer
Handler::getUser(const Wt::Auth::User& authUser)
{

	if (!authUser.isValid()) {
		LMS_LOG(DB, ERROR) << "Handler::getUser: invalid authUser";
		return User::pointer();
	}

	Wt::Dbo::ptr<AuthInfo> authInfo = _users->find(authUser);

	User::pointer user = authInfo->user();

	if (!user) {
		user = _session.add(new User());
		authInfo.modify()->setUser(user);
	}

	return user;
}

Wt::Dbo::SqlConnectionPool*
Handler::createConnectionPool(boost::filesystem::path p)
{
	LMS_LOG(DB, INFO) << "Creating connection pool on file " << p;

	Wt::Dbo::backend::Sqlite3 *connection = new Wt::Dbo::backend::Sqlite3(p.string());

	connection->executeSql("pragma journal_mode=WAL");

	//  connection->setProperty("show-queries", "true");
	return new Wt::Dbo::FixedSqlConnectionPool(connection, 1);
}


} // namespace Database
