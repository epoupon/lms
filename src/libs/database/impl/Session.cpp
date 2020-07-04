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

#include "database/Session.hpp"

#include <map>
#include <mutex>
#include <thread>

#include "utils/Exception.hpp"
#include "utils/Logger.hpp"

#include "database/Artist.hpp"
#include "database/Cluster.hpp"
#include "database/Db.hpp"
#include "database/Release.hpp"
#include "database/ScanSettings.hpp"
#include "database/Track.hpp"
#include "database/TrackBookmark.hpp"
#include "database/TrackArtistLink.hpp"
#include "database/TrackList.hpp"
#include "database/TrackFeatures.hpp"
#include "database/User.hpp"

namespace Database {

#define LMS_DATABASE_VERSION	24

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
	static const std::string outdatedMsg {"Outdated database, please rebuild it (delete the .db file and restart)"};

	Db::ScopedNoForeignKeys noPragmaKeys {_db};

	while (1)
	{
		auto uniqueTransaction {createUniqueTransaction()};

		Version version;
		try
		{
			version = VersionInfo::getOrCreate(*this)->getVersion();
			LMS_LOG(DB, INFO) << "Database version = " << version << ", LMS binary version = " << LMS_DATABASE_VERSION;
			if (version == LMS_DATABASE_VERSION)
			{
				LMS_LOG(DB, DEBUG) << "Lms database version " << LMS_DATABASE_VERSION << ": up to date!";
				return;
			}
		}
		catch (std::exception& e)
		{
			LMS_LOG(DB, ERROR) << "Cannot get database version info: " << e.what();
			throw LmsException {outdatedMsg};
		}

		LMS_LOG(DB, INFO) << "Migrating database from version " << version << "...";

		if (version == 5)
		{
			_session.execute("DELETE FROM auth_token"); // format has changed
		}
		else if (version == 6)
		{
			// Just increment the scan version of the settings to make the next scheduled scan rescan everything
			ScanSettings::get(*this).modify()->incScanVersion();
		}
		else if (version == 7)
		{
			_session.execute("DROP TABLE similarity_settings");
			_session.execute("DROP TABLE similarity_settings_feature");
			_session.execute("ALTER TABLE scan_settings ADD similarity_engine_type INTEGER NOT NULL DEFAULT(" + std::to_string(static_cast<int>(ScanSettings::RecommendationEngineType::Clusters)) + ")");
		}
		else if (version == 8)
		{
			// Better cover handling, need to rescan the whole files
			// Just increment the scan version of the settings to make the next scheduled scan rescan everything
			ScanSettings::get(*this).modify()->incScanVersion();
		}
		else if (version == 9)
		{
			_session.execute(R"(
CREATE TABLE IF NOT EXISTS "track_bookmark" (
	"id" integer primary key autoincrement,
	"version" integer not null,
	"offset" integer,
	"comment" text not null,
	"track_id" bigint,
	"user_id" bigint,
	constraint "fk_track_bookmark_track" foreign key ("track_id") references "track" ("id") on delete cascade deferrable initially deferred,
	constraint "fk_track_bookmark_user" foreign key ("user_id") references "user" ("id") on delete cascade deferrable initially deferred
);)");
		}
		else if (version == 10)
		{
			ScanSettings::get(*this).modify()->addAudioFileExtension(".m4b");
			ScanSettings::get(*this).modify()->addAudioFileExtension(".alac");
		}
		else if (version == 11)
		{
			// Sanitize bad MBID, need to rescan the whole files
			// Just increment the scan version of the settings to make the next scheduled scan rescan everything
			ScanSettings::get(*this).modify()->incScanVersion();
		}
		else if (version == 12)
		{
			// Artist and release that have a badly parsed name but a MBID had no chance to updat the name
			// Just increment the scan version of the settings to make the next scheduled scan rescan everything
			ScanSettings::get(*this).modify()->incScanVersion();
		}
		else if (version == 13)
		{
			// Always store UUID in lower case + better WMA parsing
			// Just increment the scan version of the settings to make the next scheduled scan rescan everything
			ScanSettings::get(*this).modify()->incScanVersion();
		}
		else if (version == 14)
		{
			// SortName now set from metadata
			// Just increment the scan version of the settings to make the next scheduled scan rescan everything
			ScanSettings::get(*this).modify()->incScanVersion();
		}
		else if (version == 15)
		{
			_session.execute("ALTER TABLE user ADD ui_theme INTEGER NOT NULL DEFAULT(" + std::to_string(static_cast<int>(User::defaultUITheme)) + ")");
		}
		else if (version == 16)
		{
			_session.execute("ALTER TABLE track ADD total_disc INTEGER NOT NULL DEFAULT(0)");
			_session.execute("ALTER TABLE track ADD total_track INTEGER NOT NULL DEFAULT(0)");

			// Just increment the scan version of the settings to make the next scheduled scan rescan everything
			ScanSettings::get(*this).modify()->incScanVersion();
		}
		else if (version == 17)
		{
			// Drop colums total_disc/total_track from release
			_session.execute(R"(
CREATE TABLE "release_backup" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "name" text not null,
  "mbid" text not null
))");
			_session.execute("INSERT INTO release_backup SELECT id,version,name,mbid FROM release");
			_session.execute("DROP TABLE release");
			_session.execute("ALTER TABLE release_backup RENAME TO release");

			// Just increment the scan version of the settings to make the next scheduled scan rescan everything
			ScanSettings::get(*this).modify()->incScanVersion();
		}
		else if (version == 18)
		{
			_session.execute(R"(
CREATE TABLE IF NOT EXISTS "subsonic_settings" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "api_enabled" boolean not null,
  "artist_list_mode" integer not null
))");
		}
		else if (version == 19)
		{
			_session.execute(R"(
CREATE TABLE "user_backup" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "type" integer not null,
  "login_name" text not null,
  "password_salt" text not null,
  "password_hash" text not null,
  "last_login" text,
  "subsonic_transcode_enable" boolean not null,
  "subsonic_transcode_format" integer not null,
  "subsonic_transcode_bitrate" integer not null,
  "subsonic_artist_list_mode" integer not null,
  "ui_theme" integer not null,
  "cur_playing_track_pos" integer not null,
  "repeat_all" boolean not null,
  "radio" boolean not null
))");
			_session.execute(std::string {"INSERT INTO user_backup SELECT id, version, type, login_name, password_salt, password_hash, last_login, "}
					+ (User::defaultSubsonicTranscodeEnable ? "1" : "0")
					+ ", " + std::to_string(static_cast<int>(User::defaultSubsonicTranscodeFormat))
					+ ", " + std::to_string(User::defaultSubsonicTranscodeBitrate)
					+ ", " + std::to_string(static_cast<int>(User::defaultSubsonicArtistListMode))
					+ ", ui_theme, cur_playing_track_pos, repeat_all, radio FROM user");
			_session.execute("DROP TABLE user");
			_session.execute("ALTER TABLE user_backup RENAME TO user");
		}
		else if (version == 20)
		{
			_session.execute("DROP TABLE subsonic_settings");
		}
		else if (version == 21)
		{
			_session.execute("ALTER TABLE track ADD track_replay_gain REAL");
			_session.execute("ALTER TABLE track ADD release_replay_gain REAL");

			// Just increment the scan version of the settings to make the next scheduled scan rescan everything
			ScanSettings::get(*this).modify()->incScanVersion();
		}
		else if (version == 22)
		{
			_session.execute("ALTER TABLE track ADD disc_subtitle TEXT NOT NULL DEFAULT ''");

			// Just increment the scan version of the settings to make the next scheduled scan rescan everything
			ScanSettings::get(*this).modify()->incScanVersion();
		}
		else if (version == 23)
		{
			// Better cover detection
			// Just increment the scan version of the settings to make the next scheduled scan rescan everything
			ScanSettings::get(*this).modify()->incScanVersion();
		}
		else
		{
			LMS_LOG(DB, ERROR) << "Database version " << version << " cannot be handled using migration";
			throw LmsException { LMS_DATABASE_VERSION > version  ? outdatedMsg : "Server binary outdated, please upgrade it to handle this database"};
		}

		VersionInfo::get(*this).modify()->setVersion(++version);
	}
}

Session::Session(Db& db)
: _db {db}
{
	_session.setConnectionPool(_db.getConnectionPool());

	_session.mapClass<VersionInfo>("version_info");
	_session.mapClass<Artist>("artist");
	_session.mapClass<AuthToken>("auth_token");
	_session.mapClass<Cluster>("cluster");
	_session.mapClass<ClusterType>("cluster_type");
	_session.mapClass<Release>("release");
	_session.mapClass<ScanSettings>("scan_settings");
	_session.mapClass<Track>("track");
	_session.mapClass<TrackBookmark>("track_bookmark");
	_session.mapClass<TrackArtistLink>("track_artist_link");
	_session.mapClass<TrackFeatures>("track_features");
	_session.mapClass<TrackList>("tracklist");
	_session.mapClass<TrackListEntry>("tracklist_entry");
	_session.mapClass<User>("user");
}


enum class OwnedLock
{
	None,
	Shared,
	Unique,
};

static thread_local std::map<std::shared_mutex*, OwnedLock> lockDebug;

UniqueTransaction::UniqueTransaction(std::shared_mutex& mutex, Wt::Dbo::Session& session)
: _lock {mutex},
 _transaction {session}
{
	assert(lockDebug[_lock.mutex()] == OwnedLock::None);
	lockDebug[_lock.mutex()] = OwnedLock::Unique;
}

UniqueTransaction::~UniqueTransaction()
{
	assert(lockDebug[_lock.mutex()] == OwnedLock::Unique);
	lockDebug[_lock.mutex()] = OwnedLock::None;
}

SharedTransaction::SharedTransaction(std::shared_mutex& mutex, Wt::Dbo::Session& session)
: _lock {mutex},
 _transaction {session}
{
	assert(lockDebug[_lock.mutex()] == OwnedLock::None);
	lockDebug[_lock.mutex()] = OwnedLock::Shared;
}

SharedTransaction::~SharedTransaction()
{
	assert(lockDebug[_lock.mutex()] == OwnedLock::Shared);
	lockDebug[_lock.mutex()] = OwnedLock::None;
}

void
Session::checkUniqueLocked()
{
	assert(lockDebug[&_db.getMutex()] == OwnedLock::Unique);
}

void
Session::checkSharedLocked()
{
	assert(lockDebug[&_db.getMutex()] != OwnedLock::None);
}

UniqueTransaction
Session::createUniqueTransaction()
{
	return UniqueTransaction{_db.getMutex(), _session};
}

SharedTransaction
Session::createSharedTransaction()
{
	return SharedTransaction{_db.getMutex(), _session};
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
		_session.execute("CREATE INDEX IF NOT EXISTS auth_token_user_idx ON auth_token(user_id)");
		_session.execute("CREATE INDEX IF NOT EXISTS auth_token_expiry_idx ON auth_token(expiry)");
		_session.execute("CREATE INDEX IF NOT EXISTS auth_token_value_idx ON auth_token(value)");
		_session.execute("CREATE INDEX IF NOT EXISTS cluster_name_idx ON cluster(name)");
		_session.execute("CREATE INDEX IF NOT EXISTS cluster_cluster_type_idx ON cluster(cluster_type_id)");
		_session.execute("CREATE INDEX IF NOT EXISTS cluster_type_name_idx ON cluster_type(name)");
		_session.execute("CREATE INDEX IF NOT EXISTS release_name_idx ON release(name)");
		_session.execute("CREATE INDEX IF NOT EXISTS release_name_nocase_idx ON release(name COLLATE NOCASE)");
		_session.execute("CREATE INDEX IF NOT EXISTS release_mbid_idx ON release(mbid)");
		_session.execute("CREATE INDEX IF NOT EXISTS track_file_last_write_idx ON track(file_last_write)");
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
		_session.execute("CREATE INDEX IF NOT EXISTS track_bookmark_user_idx ON track_bookmark(user_id)");
		_session.execute("CREATE INDEX IF NOT EXISTS track_bookmark_user_track_idx ON track_bookmark(user_id,track_id)");
	}

	// Initial settings tables
	{
		auto uniqueTransaction {createUniqueTransaction()};

		ScanSettings::init(*this);
	}
}

void
Session::optimize()
{
	LMS_LOG(DB, DEBUG) << "Optimizing db...";
	{
		auto uniqueTransaction {createUniqueTransaction()};
		_session.execute("ANALYZE");
	}
	LMS_LOG(DB, DEBUG) << "Optimized db!";
}

} // namespace Database
