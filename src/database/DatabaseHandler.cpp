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

#include <Wt/Dbo/FixedSqlConnectionPool.h>
#include <Wt/Dbo/backend/Sqlite3.h>

#include <Wt/Auth/Dbo/AuthInfo.h>
#include <Wt/Auth/Dbo/UserDatabase.h>
#include <Wt/Auth/AuthService.h>
#include <Wt/Auth/HashFunction.h>
#include <Wt/Auth/Identity.h>
#include <Wt/Auth/PasswordService.h>
#include <Wt/Auth/PasswordStrengthValidator.h>
#include <Wt/Auth/PasswordVerifier.h>

#include "Setting.hpp"

#include "utils/Logger.hpp"

#include "DbArtist.hpp"
#include "MediaDirectory.hpp"
#include "Playlist.hpp"
#include "Release.hpp"
#include "Track.hpp"

#include "DatabaseHandler.hpp"

namespace Database {


namespace {
	Wt::Auth::AuthService authService;
	Wt::Auth::PasswordService passwordService(authService);
}


void
Handler::configureAuth(void)
{
	authService.setEmailVerificationEnabled(false);
	authService.setAuthTokensEnabled(true, "lmsauth");
	authService.setIdentityPolicy(Wt::Auth::IdentityPolicy::LoginName);
	authService.setRandomTokenLength(32);

#if WT_VERSION < 0X04000300
	authService.setTokenHashFunction(new Wt::Auth::BCryptHashFunction(8));
#else
	authService.setTokenHashFunction(std::make_unique<Wt::Auth::BCryptHashFunction>(8));
#endif

	auto verifier = std::make_unique<Wt::Auth::PasswordVerifier>();
	verifier->addHashFunction(std::make_unique<Wt::Auth::BCryptHashFunction>(8));
	passwordService.setVerifier(std::move(verifier));
	passwordService.setAttemptThrottlingEnabled(true);

	auto strengthValidator = std::make_unique<Wt::Auth::PasswordStrengthValidator>();
	// Reduce some constraints...
	strengthValidator->setMinimumLength( Wt::Auth::PasswordStrengthType::PassPhrase, 4);
	strengthValidator->setMinimumLength( Wt::Auth::PasswordStrengthType::OneCharClass, 4);
	strengthValidator->setMinimumLength( Wt::Auth::PasswordStrengthType::TwoCharClass, 4);
	strengthValidator->setMinimumLength( Wt::Auth::PasswordStrengthType::ThreeCharClass, 4 );
	strengthValidator->setMinimumLength( Wt::Auth::PasswordStrengthType::FourCharClass, 4 );

	passwordService.setStrengthValidator(std::move(strengthValidator));
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
	_session.mapClass<Database::ClusterType>("cluster_type");
	_session.mapClass<Database::MediaDirectory>("media_directory");
	_session.mapClass<Database::Playlist>("playlist");
	_session.mapClass<Database::PlaylistEntry>("playlist_entry");
	_session.mapClass<Database::Release>("release");
	_session.mapClass<Database::Setting>("setting");
	_session.mapClass<Database::Track>("track");

	_session.mapClass<Database::AuthInfo>("auth_info");
	_session.mapClass<Database::AuthInfo::AuthIdentityType>("auth_identity");
	_session.mapClass<Database::AuthInfo::AuthTokenType>("auth_token");
	_session.mapClass<Database::User>("user");

	try {
		Wt::Dbo::Transaction transaction(_session);

	        _session.createTables();

		LMS_LOG(DB, INFO) << "Tables created";
	}
	catch(std::exception& e) {
		LMS_LOG(DB, ERROR) << "Cannot create tables: " << e.what();
	}

	{
		Wt::Dbo::Transaction transaction(_session);

		// Indexes
		_session.execute("CREATE INDEX IF NOT EXISTS track_path_idx ON track(file_path)");
		_session.execute("CREATE INDEX IF NOT EXISTS artist_name_idx ON artist(name)");
		_session.execute("CREATE INDEX IF NOT EXISTS release_name_idx ON release(name)");
		_session.execute("CREATE INDEX IF NOT EXISTS track_artist_idx ON track(artist_id)");
		_session.execute("CREATE INDEX IF NOT EXISTS track_release_idx ON track(release_id)");
		_session.execute("CREATE INDEX IF NOT EXISTS cluster_name_idx ON cluster(name)");
		_session.execute("CREATE INDEX IF NOT EXISTS cluster_type_name_idx ON cluster_type(name)");
		_session.execute("CREATE INDEX IF NOT EXISTS settings_name_idx ON setting(name)");
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

	return authInfo->user();
}

User::pointer
Handler::createUser(const Wt::Auth::User& authUser)
{
	if (!authUser.isValid()) {
		LMS_LOG(DB, ERROR) << "Handler::getUser: invalid authUser";
		return User::pointer();
	}

	User::pointer user = _session.add(std::make_unique<User>());
	Wt::Dbo::ptr<AuthInfo> authInfo = _users->find(authUser);
	authInfo.modify()->setUser(user);

	return user;
}

std::unique_ptr<Wt::Dbo::SqlConnectionPool>
Handler::createConnectionPool(boost::filesystem::path p)
{
	LMS_LOG(DB, INFO) << "Creating connection pool on file " << p.string();

	auto connection = std::make_unique<Wt::Dbo::backend::Sqlite3>(p.string());
	connection->executeSql("pragma journal_mode=WAL");
	connection->setProperty("show-queries", "true");

	auto pool = std::make_unique<Wt::Dbo::FixedSqlConnectionPool>(std::move(connection), 1);
	pool->setTimeout(std::chrono::seconds(10));

	return pool;
}


} // namespace Database
