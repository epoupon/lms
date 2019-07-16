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

#include "Session.hpp"

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
#include "User.hpp"

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

		static VersionInfo::pointer getOrCreate(Session& session)
		{
			session.checkUniqueLocked();

			pointer versionInfo {session.getDboSession().find<VersionInfo>()};
			if (!versionInfo)
				return session.getDboSession().add(std::make_unique<VersionInfo>());

			return versionInfo;
		}

		static VersionInfo::pointer get(Session& session)
		{
			session.checkSharedLocked();

			return session.getDboSession().find<VersionInfo>();
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

void
Session::doDatabaseMigrationIfNeeded()
{
	auto uniqueTransaction {createUniqueTransaction()};

	static const std::string outdatedMsg {"Outdated database, please rebuild it (delete the .db file and restart)"};

	Version version;
	try
	{
		version = VersionInfo::getOrCreate(*this)->getVersion();
		LMS_LOG(DB, INFO) << "Database version = " << version;
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

			_session.execute(R"(CREATE TABLE IF NOT EXISTS "user_artist_starred" (
	"user_id" bigint,
	"artist_id" bigint,
	primary key ("user_id", "artist_id"),
	constraint "fk_user_artist_starred_key1" foreign key ("user_id") references "user" ("id") on delete cascade deferrable initially deferred,
	constraint "fk_user_artist_starred_key2" foreign key ("artist_id") references "artist" ("id") deferrable initially deferred);)");
			_session.execute(R"(CREATE INDEX "user_artist_starred_user" on "user_artist_starred" ("user_id");)");
			_session.execute(R"(CREATE INDEX "user_artist_starred_artist" on "user_artist_starred" ("artist_id");)");
			_session.execute(R"(CREATE TABLE IF NOT EXISTS "user_release_starred" (
	"user_id" bigint,
	"release_id" bigint,
	primary key ("user_id", "release_id"),
	constraint "fk_user_release_starred_key1" foreign key ("user_id") references "user" ("id") on delete cascade deferrable initially deferred,
	constraint "fk_user_release_starred_key2" foreign key ("release_id") references "release" ("id") on delete cascade deferrable initially deferred);)");
			_session.execute(R"(CREATE INDEX "user_release_starred_user" on "user_release_starred" ("user_id");)");
			_session.execute(R"(CREATE INDEX "user_release_starred_release" on "user_release_starred" ("release_id");)");
			_session.execute(R"(CREATE TABLE IF NOT EXISTS "user_track_starred" (
	"user_id" bigint,
	"track_id" bigint,
	primary key ("user_id", "track_id"),
	constraint "fk_user_track_starred_key1" foreign key ("user_id") references "user" ("id") on delete cascade deferrable initially deferred,
	constraint "fk_user_track_starred_key2" foreign key ("track_id") references "track" ("id") on delete cascade deferrable initially deferred);)");
			_session.execute(R"(CREATE INDEX "user_track_starred_user" on "user_track_starred" ("user_id");)");
			_session.execute(R"(CREATE INDEX "user_track_starred_track" on "user_track_starred" ("track_id");)");
		break;

		default:
			LMS_LOG(DB, ERROR) << "Database version " << version << " cannot be handled using migration";
			throw LmsException {outdatedMsg};
	}

	VersionInfo::get(*this).modify()->setVersion(LMS_DATABASE_VERSION);
}


void
Session::configureAuth(void)
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
Session::getAuthService()
{
	return authService;
}

const Wt::Auth::PasswordService&
Session::getPasswordService()
{
	return passwordService;
}


Session::Session(std::shared_timed_mutex& mutex, Wt::Dbo::SqlConnectionPool& connectionPool)
: _mutex {mutex}
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

	_users = std::make_unique<UserDatabase>(_session);
}

// TODO make this per database
static thread_local bool hasSharedLock {false};
static thread_local bool hasUniqueLock {false};

UniqueTransaction::UniqueTransaction(std::shared_timed_mutex& mutex, Wt::Dbo::Session& session)
: _lock {mutex},
 _transaction {session}
{
	assert(!hasSharedLock);
	assert(!hasUniqueLock);
	hasUniqueLock = true;
	LMS_LOG(DB, DEBUG) << "UniqueTransaction ACQUIRED";
}

UniqueTransaction::~UniqueTransaction()
{
	assert(hasUniqueLock);
	hasUniqueLock = false;
	LMS_LOG(DB, DEBUG) << "UniqueTransaction RELEASED";
}

SharedTransaction::SharedTransaction(std::shared_timed_mutex& mutex, Wt::Dbo::Session& session)
: _lock {mutex},
 _transaction {session}
{
	assert(!hasSharedLock);
	assert(!hasUniqueLock);
	hasSharedLock = true;
	LMS_LOG(DB, DEBUG) << "SharedTransaction ACQUIRED";
}

SharedTransaction::~SharedTransaction()
{
	assert(hasSharedLock);
	hasSharedLock = false;
	LMS_LOG(DB, DEBUG) << "SharedTransaction RELEASED";
}

void
Session::checkUniqueLocked()
{
	assert(hasUniqueLock);
}

void
Session::checkSharedLocked()
{
	assert(hasUniqueLock || hasSharedLock);
}

std::unique_ptr<UniqueTransaction>
Session::createUniqueTransaction()
{
	return std::unique_ptr<UniqueTransaction>(new UniqueTransaction{_mutex, _session});
}

std::unique_ptr<SharedTransaction>
Session::createSharedTransaction()
{
	return std::unique_ptr<SharedTransaction>(new SharedTransaction{_mutex, _session});
}

void
Session::prepareTables()
{
	// Creation case
	try {
	        _session.createTables();

		LMS_LOG(DB, INFO) << "Tables created";
	}
	catch (Wt::Dbo::Exception& e)
	{
		LMS_LOG(DB, ERROR) << "Cannot create tables: " << e.what();
	}

	doDatabaseMigrationIfNeeded();

	// Indexes
	{
		auto uniqueTransaction {createUniqueTransaction()};
		_session.execute("CREATE INDEX IF NOT EXISTS artist_name_idx ON artist(name)");
		_session.execute("CREATE INDEX IF NOT EXISTS artist_sort_name_nocase_idx ON artist(sort_name COLLATE NOCASE)");
		_session.execute("CREATE INDEX IF NOT EXISTS artist_mbid_idx ON artist(mbid)");
		_session.execute("CREATE INDEX IF NOT EXISTS cluster_name_idx ON cluster(name)");
		_session.execute("CREATE INDEX IF NOT EXISTS cluster_cluster_type_idx ON cluster(cluster_type_id)");
		_session.execute("CREATE INDEX IF NOT EXISTS cluster_type_name_idx ON cluster_type(name)");
		_session.execute("CREATE INDEX IF NOT EXISTS release_name_idx ON release(name)");
		_session.execute("CREATE INDEX IF NOT EXISTS release_name_nocase_idx ON release(name COLLATE NOCASE)");
		_session.execute("CREATE INDEX IF NOT EXISTS release_mbid_idx ON release(mbid)");
		_session.execute("CREATE INDEX IF NOT EXISTS track_path_idx ON track(file_path)");
		_session.execute("CREATE INDEX IF NOT EXISTS track_name_idx ON track(name)");
		_session.execute("CREATE INDEX IF NOT EXISTS track_name_nocase_idx ON track(name COLLATE NOCASE)");
		_session.execute("CREATE INDEX IF NOT EXISTS track_mbid_idx ON track(mbid)");
		_session.execute("CREATE INDEX IF NOT EXISTS track_release_idx ON track(release_id)");
		_session.execute("CREATE INDEX IF NOT EXISTS track_year_idx ON track(year)");
		_session.execute("CREATE INDEX IF NOT EXISTS track_original_year_idx ON track(original_year)");
		_session.execute("CREATE INDEX IF NOT EXISTS tracklist_name_idx ON tracklist(name)");
		_session.execute("CREATE INDEX IF NOT EXISTS tracklist_user_idx ON tracklist(user_id)");
		_session.execute("CREATE INDEX IF NOT EXISTS track_features_track_idx ON track_features(track_id)");
		_session.execute("CREATE INDEX IF NOT EXISTS track_artist_link_artist_idx ON track_artist_link(artist_id)");
		_session.execute("CREATE INDEX IF NOT EXISTS track_artist_link_name_idx ON track_artist_link(name)");
		_session.execute("CREATE INDEX IF NOT EXISTS track_artist_link_track_idx ON track_artist_link(track_id)");
		_session.execute("CREATE INDEX IF NOT EXISTS track_artist_link_type_idx ON track_artist_link(type)");
	}

	// Initial settings tables
	{
		auto uniqueTransaction {createUniqueTransaction()};

		ScanSettings::init(*this);
		SimilaritySettings::init(*this);
	}
}

void
Session::optimize()
{
	auto uniqueTransaction {createUniqueTransaction()};

	_session.execute("ANALYZE");
}

std::string
Session::getUserLoginName(Wt::Dbo::ptr<User> user)
{
	const Wt::Auth::User authUser {_users->findWithId(std::to_string(user.id()))};
	if (!authUser.isValid())
		throw LmsException {"Invalid user state"};

	return authUser.identity(Wt::Auth::Identity::LoginName).toUTF8();
}

bool
Session::checkUserPassword(const std::string& loginName, const std::string& password)
{
	auto transaction {createUniqueTransaction()};

	auto authUser {_users->findWithIdentity(Wt::Auth::Identity::LoginName, loginName)};
	if (!authUser.isValid())
		return false;	// TODO const time?

	return passwordService.verifyPassword(authUser, password) == Wt::Auth::PasswordResult::PasswordValid;
}

void
Session::updateUserPassword(Wt::Dbo::ptr<User> user, const std::string& password)
{
	const Wt::Auth::User authUser {_users->findWithId(std::to_string(user.id()))};
	if (!authUser.isValid())
		throw LmsException {"Bad user state"};
	passwordService.updatePassword(authUser, password);
}

Wt::WDateTime
Session::getUserLastLoginAttempt(Wt::Dbo::ptr<User> user)
{
	const Wt::Auth::User authUser {_users->findWithId(std::to_string(user.id()))};
	if (!authUser.isValid())
		throw LmsException {"Bad user state"};

	return authUser.lastLoginAttempt();
}

void
Session::removeUser(Database::User::pointer user)
{
	checkUniqueLocked();

	auto authUser = _users->findWithId(std::to_string(user.id()));
	_users->deleteUser(authUser);
	user.remove();
}

Wt::Auth::AbstractUserDatabase&
Session::getUserDatabase()
{
	return *_users;
}

User::pointer
Session::createUser(const std::string& loginName, const std::string& password)
{
	Wt::Auth::User authUser {_users->registerNew()};
	if (!authUser.isValid())
	{
		LMS_LOG(DB, ERROR) << "Invalid authUser";
		return {};
	}
	User::pointer user {User::create(*this)};
	Wt::Dbo::ptr<AuthInfo> authInfo = _users->find(authUser);
	authInfo.modify()->setUser(user);

	authUser.setIdentity(Wt::Auth::Identity::LoginName, loginName);
	passwordService.updatePassword(authUser, password);

	return user;
}

User::pointer
Session::getLoggedUser()
{
	if (_login.loggedIn())
		return getUser(_login.user());
	else
		return User::pointer();
}

User::pointer
Session::getUser(const Wt::Auth::User& authUser)
{
	if (!authUser.isValid()) {
		LMS_LOG(DB, ERROR) << "Session::getUser: invalid authUser";
		return User::pointer();
	}

	Wt::Dbo::ptr<AuthInfo> authInfo = _users->find(authUser);

	return authInfo->user();
}

User::pointer
Session::getUser(const std::string& loginName)
{
	auto authUser {getUserDatabase().findWithIdentity(Wt::Auth::Identity::LoginName, loginName)};
	if (!authUser.isValid())
		return User::pointer {};

	return getUser(authUser);
}

} // namespace Database
