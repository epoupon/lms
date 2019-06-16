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

#include "DatabaseHandler.hpp"

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

#include "utils/Exception.hpp"
#include "utils/Logger.hpp"

#include "Artist.hpp"
#include "Cluster.hpp"
#include "Release.hpp"
#include "ScanSettings.hpp"
#include "SimilaritySettings.hpp"
#include "Track.hpp"
#include "TrackArtistLink.hpp"
#include "TrackList.hpp"
#include "TrackFeatures.hpp"

namespace Database {

#define LMS_DATABASE_VERSION	4

namespace {
	Wt::Auth::AuthService authService;
	Wt::Auth::PasswordService passwordService {authService};
}

using Version = std::size_t;

class VersionInfo
{
	public:
		using pointer = Wt::Dbo::ptr<VersionInfo>;

		static VersionInfo::pointer get(Wt::Dbo::Session& session)
		{
			pointer versionInfo {session.find<VersionInfo>()};
			if (!versionInfo)
				versionInfo = session.add(std::make_unique<VersionInfo>());

			return versionInfo;
		}

		Version getVersion() const { return _version; }
		void setVersion(Version version) { _version = static_cast<int>(version); }

		template<class Action>
		void persist(Action& a)
		{
			Wt::Dbo::field(a, _version, "db_version");
		}

	private:
		int _version {LMS_DATABASE_VERSION};
};

static
void
doDatabaseMigrationIfNeeded(Wt::Dbo::Session& session)
{
	Wt::Dbo::Transaction transaction {session};

	static const std::string outdatedMsg {"Outdated database, please rebuild it (delete the .db file and restart)"};

	Version version;
	try
	{
		version = VersionInfo::get(session)->getVersion();
		if (version == LMS_DATABASE_VERSION)
			return;
	}
	catch (std::exception& e)
	{
		LMS_LOG(DB, ERROR) << "Cannot get database version info: " << e.what();
		throw LmsException {outdatedMsg};
	}

	switch (version)
	{
		case 3:

			LMS_LOG(DB, INFO) << "Migrating database from version 3...";

			session.execute(R"(CREATE TABLE IF NOT EXISTS "user_artist_starred" (
	"user_id" bigint,
	"artist_id" bigint,
	primary key ("user_id", "artist_id"),
	constraint "fk_user_artist_starred_key1" foreign key ("user_id") references "user" ("id") on delete cascade deferrable initially deferred,
	constraint "fk_user_artist_starred_key2" foreign key ("artist_id") references "artist" ("id") deferrable initially deferred);)");
			session.execute(R"(CREATE INDEX "user_artist_starred_user" on "user_artist_starred" ("user_id");)");
			session.execute(R"(CREATE INDEX "user_artist_starred_artist" on "user_artist_starred" ("artist_id");)");
			session.execute(R"(CREATE TABLE IF NOT EXISTS "user_release_starred" (
	"user_id" bigint,
	"release_id" bigint,
	primary key ("user_id", "release_id"),
	constraint "fk_user_release_starred_key1" foreign key ("user_id") references "user" ("id") on delete cascade deferrable initially deferred,
	constraint "fk_user_release_starred_key2" foreign key ("release_id") references "release" ("id") on delete cascade deferrable initially deferred);)");
			session.execute(R"(CREATE INDEX "user_release_starred_user" on "user_release_starred" ("user_id");)");
			session.execute(R"(CREATE INDEX "user_release_starred_release" on "user_release_starred" ("release_id");)");
			session.execute(R"(CREATE TABLE IF NOT EXISTS "user_track_starred" (
	"user_id" bigint,
	"track_id" bigint,
	primary key ("user_id", "track_id"),
	constraint "fk_user_track_starred_key1" foreign key ("user_id") references "user" ("id") on delete cascade deferrable initially deferred,
	constraint "fk_user_track_starred_key2" foreign key ("track_id") references "track" ("id") on delete cascade deferrable initially deferred);)");
			session.execute(R"(CREATE INDEX "user_track_starred_user" on "user_track_starred" ("user_id");)");
			session.execute(R"(CREATE INDEX "user_track_starred_track" on "user_track_starred" ("track_id");)");
		break;

		default:
			LMS_LOG(DB, ERROR) << "Database version " << version << " cannot be handled using migration";
			throw LmsException {outdatedMsg};
	}

	VersionInfo::get(session).modify()->setVersion(LMS_DATABASE_VERSION);
}


void
Handler::configureAuth(void)
{
	authService.setEmailVerificationEnabled(false);
	authService.setAuthTokensEnabled(true, "lmsauth");
	authService.setAuthTokenValidity(24 * 60 * 365); // A year
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

	_session.mapClass<VersionInfo>("version_info");
	_session.mapClass<Artist>("artist");
	_session.mapClass<Cluster>("cluster");
	_session.mapClass<ClusterType>("cluster_type");
	_session.mapClass<TrackList>("tracklist");
	_session.mapClass<TrackListEntry>("tracklist_entry");
	_session.mapClass<Release>("release");
	_session.mapClass<Track>("track");
	_session.mapClass<TrackArtistLink>("track_artist_link");
	_session.mapClass<TrackFeatures>("track_features");

	_session.mapClass<ScanSettings>("scan_settings");
	_session.mapClass<SimilaritySettings>("similarity_settings");
	_session.mapClass<SimilaritySettingsFeature>("similarity_settings_feature");

	_session.mapClass<AuthInfo>("auth_info");
	_session.mapClass<AuthInfo::AuthIdentityType>("auth_identity");
	_session.mapClass<AuthInfo::AuthTokenType>("auth_token");
	_session.mapClass<User>("user");

	try {
		Wt::Dbo::Transaction transaction {_session};

	        _session.createTables();

		LMS_LOG(DB, INFO) << "Tables created";
	}
	catch (Wt::Dbo::Exception& e)
	{
		LMS_LOG(DB, ERROR) << "Cannot create tables: " << e.what();
	}

	doDatabaseMigrationIfNeeded(_session);

	{
		Wt::Dbo::Transaction transaction {_session};

		// Indexes
		_session.execute("CREATE INDEX IF NOT EXISTS track_path_idx ON track(file_path)");
		_session.execute("CREATE INDEX IF NOT EXISTS track_name_idx ON track(name)");
		_session.execute("CREATE INDEX IF NOT EXISTS artist_name_idx ON artist(name)");
		_session.execute("CREATE INDEX IF NOT EXISTS release_name_idx ON release(name)");
		_session.execute("CREATE INDEX IF NOT EXISTS track_release_idx ON track(release_id)");
		_session.execute("CREATE INDEX IF NOT EXISTS cluster_name_idx ON cluster(name)");
		_session.execute("CREATE INDEX IF NOT EXISTS cluster_type_name_idx ON cluster_type(name)");
		_session.execute("CREATE INDEX IF NOT EXISTS tracklist_name_idx ON tracklist(name)");
		_session.execute("CREATE INDEX IF NOT EXISTS track_features_track_idx ON track_features(track_id)");
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
Handler::getUser(const std::string& loginName)
{
	auto authUser {getUserDatabase().findWithIdentity(Wt::Auth::Identity::LoginName, loginName)};
	if (!authUser.isValid())
		return User::pointer {};

	return getUser(authUser);
}

User::pointer
Handler::createUser(const Wt::Auth::User& authUser)
{
	if (!authUser.isValid())
	{
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
//	connection->setProperty("show-queries", "true");

	auto pool = std::make_unique<Wt::Dbo::FixedSqlConnectionPool>(std::move(connection), 1);
	pool->setTimeout(std::chrono::seconds(10));

	return pool;
}


} // namespace Database
