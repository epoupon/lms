/*
 * Copyright (C) 2020 Emeric Poupon
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

#include "Migration.hpp"

#include <Wt/Dbo/WtSqlTraits.h>

#include "services/database/Db.hpp"
#include "services/database/ScanSettings.hpp"
#include "services/database/Session.hpp"
#include "services/database/User.hpp"
#include "utils/Exception.hpp"
#include "utils/Logger.hpp"

namespace Database
{
    VersionInfo::pointer VersionInfo::getOrCreate(Session& session)
    {
        session.checkUniqueLocked();

        pointer versionInfo{ session.getDboSession().find<VersionInfo>() };
        if (!versionInfo)
            return session.getDboSession().add(std::make_unique<VersionInfo>());

        return versionInfo;
    }

    VersionInfo::pointer VersionInfo::get(Session& session)
    {
        session.checkSharedLocked();

        return session.getDboSession().find<VersionInfo>();
    }
}

namespace Database::Migration
{
    class ScopedNoForeignKeys
    {
    public:
        ScopedNoForeignKeys(Db& db) : _db{ db }
        {
            _db.executeSql("PRAGMA foreign_keys=OFF");
        }
        ~ScopedNoForeignKeys()
        {
            _db.executeSql("PRAGMA foreign_keys=ON");
        }

        ScopedNoForeignKeys(const ScopedNoForeignKeys&) = delete;
        ScopedNoForeignKeys(ScopedNoForeignKeys&&) = delete;
        ScopedNoForeignKeys& operator=(const ScopedNoForeignKeys&) = delete;
        ScopedNoForeignKeys& operator=(ScopedNoForeignKeys&&) = delete;

    private:
        Db& _db;
    };

    static std::string dateTimeToDbFormat(const Wt::WDateTime& dateTime)
    {
        return dateTime.toString("yyyy'-'MM'-'dd'T'hh':'mm':'ss'.000'", false).toUTF8();
    }

    static void migrateFromV5(Session& session)
    {
        session.getDboSession().execute("DELETE FROM auth_token"); // format has changed
    }

    static void migrateFromV6(Session& session)
    {
        // Just increment the scan version of the settings to make the next scheduled scan rescan everything
        ScanSettings::get(session).modify()->incScanVersion();
    }


    static void migrateFromV7(Session& session)
    {
        session.getDboSession().execute("DROP TABLE similarity_settings");
        session.getDboSession().execute("DROP TABLE similarity_settings_feature");
        session.getDboSession().execute("ALTER TABLE scan_settings ADD similarity_engine_type INTEGER NOT NULL DEFAULT(" + std::to_string(static_cast<int>(ScanSettings::RecommendationEngineType::Clusters)) + ")");
    }

    static void migrateFromV8(Session& session)
    {
        // Better cover handling, need to rescan the whole files
        // Just increment the scan version of the settings to make the next scheduled scan rescan everything
        ScanSettings::get(session).modify()->incScanVersion();
    }

    static void migrateFromV9(Session& session)
    {
        session.getDboSession().execute(R"(
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

    static void migrateFromV10(Session& session)
    {
        ScanSettings::get(session).modify()->addAudioFileExtension(".m4b");
        ScanSettings::get(session).modify()->addAudioFileExtension(".alac");
    }

    static void migrateFromV11(Session& session)
    {
        // Sanitize bad MBID, need to rescan the whole files
        // Just increment the scan version of the settings to make the next scheduled scan rescan everything
        ScanSettings::get(session).modify()->incScanVersion();
    }

    static void migrateFromV12(Session& session)
    {
        // Artist and release that have a badly parsed name but a MBID had no chance to updat the name
        // Just increment the scan version of the settings to make the next scheduled scan rescan everything
        ScanSettings::get(session).modify()->incScanVersion();
    }

    static void migrateFromV13(Session& session)
    {
        // Always store UUID in lower case + better WMA parsing
        // Just increment the scan version of the settings to make the next scheduled scan rescan everything
        ScanSettings::get(session).modify()->incScanVersion();
    }

    static void migrateFromV14(Session& session)
    {
        // SortName now set from metadata
        // Just increment the scan version of the settings to make the next scheduled scan rescan everything
        ScanSettings::get(session).modify()->incScanVersion();
    }

    static void migrateFromV15(Session& session)
    {
        session.getDboSession().execute("ALTER TABLE user ADD ui_theme INTEGER NOT NULL DEFAULT(" + std::to_string(static_cast<int>(User::defaultUITheme)) + ")");
    }

    static void migrateFromV16(Session& session)
    {
        session.getDboSession().execute("ALTER TABLE track ADD total_disc INTEGER NOT NULL DEFAULT(0)");
        session.getDboSession().execute("ALTER TABLE track ADD total_track INTEGER NOT NULL DEFAULT(0)");

        // Just increment the scan version of the settings to make the next scheduled scan rescan everything
        ScanSettings::get(session).modify()->incScanVersion();
    }

    static void migrateFromV17(Session& session)
    {
        // Drop colums total_disc/total_track from release
        session.getDboSession().execute(R"(
CREATE TABLE "release_backup" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "name" text not null,
  "mbid" text not null
))");
        session.getDboSession().execute("INSERT INTO release_backup SELECT id,version,name,mbid FROM release");
        session.getDboSession().execute("DROP TABLE release");
        session.getDboSession().execute("ALTER TABLE release_backup RENAME TO release");

        // Just increment the scan version of the settings to make the next scheduled scan rescan everything
        ScanSettings::get(session).modify()->incScanVersion();
    }

    static void migrateFromV18(Session& session)
    {
        session.getDboSession().execute(R"(
CREATE TABLE IF NOT EXISTS "subsonic_settings" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "api_enabled" boolean not null,
  "artist_list_mode" integer not null
))");
    }

    static void migrateFromV19(Session& session)
    {
        session.getDboSession().execute(R"(
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
        session.getDboSession().execute(std::string{ "INSERT INTO user_backup SELECT id, version, type, login_name, password_salt, password_hash, last_login, " }
            + "1" // default enable transcode
            + ", " + std::to_string(static_cast<int>(User::defaultSubsonicTranscodeFormat))
            + ", " + std::to_string(User::defaultSubsonicTranscodeBitrate)
            + ", " + std::to_string(static_cast<int>(User::defaultSubsonicArtistListMode))
            + ", ui_theme, cur_playing_track_pos, repeat_all, radio FROM user");
        session.getDboSession().execute("DROP TABLE user");
        session.getDboSession().execute("ALTER TABLE user_backup RENAME TO user");
    }

    static void migrateFromV20(Session& session)
    {
        session.getDboSession().execute("DROP TABLE subsonic_settings");
    }

    static void migrateFromV21(Session& session)
    {
        session.getDboSession().execute("ALTER TABLE track ADD track_replay_gain REAL");
        session.getDboSession().execute("ALTER TABLE track ADD release_replay_gain REAL");

        // Just increment the scan version of the settings to make the next scheduled scan rescan everything
        ScanSettings::get(session).modify()->incScanVersion();
    }

    static void migrateFromV22(Session& session)
    {
        session.getDboSession().execute("ALTER TABLE track ADD disc_subtitle TEXT NOT NULL DEFAULT ''");

        // Just increment the scan version of the settings to make the next scheduled scan rescan everything
        ScanSettings::get(session).modify()->incScanVersion();
    }

    static void migrateFromV23(Session& session)
    {
        // Better cover detection
        // Just increment the scan version of the settings to make the next scheduled scan rescan everything
        ScanSettings::get(session).modify()->incScanVersion();
    }

    static void migrateFromV24(Session& session)
    {
        // User's AuthMode
        session.getDboSession().execute("ALTER TABLE user ADD auth_mode INTEGER NOT NULL DEFAULT(" + std::to_string(static_cast<int>(/*User::defaultAuthMode*/0)) + ")");
    }

    static void migrateFromV25(Session& session)
    {
        // Better cover detection
        // Just increment the scan version of the settings to make the next scheduled scan rescan everything
        ScanSettings::get(session).modify()->incScanVersion();
    }

    static void migrateFromV26(Session& session)
    {
        // Composer, mixer, etc. support
        // Just increment the scan version of the settings to make the next scheduled scan rescan everything
        ScanSettings::get(session).modify()->incScanVersion();
    }

    static void migrateFromV27(Session& session)
    {
        // Composer, mixer, etc. support, now fallback on MBID tagged entries as there is no mean to provide MBID by tags for these kinf od artists
        // Just increment the scan version of the settings to make the next scheduled scan rescan everything
        ScanSettings::get(session).modify()->incScanVersion();
    }

    static void migrateFromV28(Session& session)
    {
        // Drop Auth mode
        session.getDboSession().execute(R"(
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
        session.getDboSession().execute("INSERT INTO user_backup SELECT id, version, type, login_name, password_salt, password_hash, last_login, subsonic_transcode_enable, subsonic_transcode_format, subsonic_transcode_bitrate, subsonic_artist_list_mode, ui_theme, cur_playing_track_pos, repeat_all, radio FROM user");
        session.getDboSession().execute("DROP TABLE user");
        session.getDboSession().execute("ALTER TABLE user_backup RENAME TO user");
    }

    static void migrateFromV29(Session& session)
    {
        session.getDboSession().execute("ALTER TABLE tracklist_entry ADD date_time TEXT");
        session.getDboSession().execute("ALTER TABLE user ADD listenbrainz_token TEXT");
        session.getDboSession().execute("ALTER TABLE user ADD scrobbler INTEGER NOT NULL DEFAULT(" + std::to_string(static_cast<int>(User::defaultScrobbler)) + ")");
        session.getDboSession().execute("ALTER TABLE track ADD recording_mbid TEXT");

        session.getDboSession().execute("DELETE from tracklist WHERE name = ?").bind("__played_tracks__");

        // MBID changes
        // Just increment the scan version of the settings to make the next scheduled scan rescan everything
        ScanSettings::get(session).modify()->incScanVersion();
    }

    static void migrateFromV30(Session& session)
    {
        // drop "year" and "original_year" (rescan needed to convert them into dates)
        session.getDboSession().execute(R"(
CREATE TABLE "track_backup" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "scan_version" integer not null,
  "track_number" integer not null,
  "disc_number" integer not null,
  "name" text not null,
  "duration" integer,
  "date" integer text,
  "original_date" integer text,
  "file_path" text not null,
  "file_last_write" text,
  "file_added" text,
  "has_cover" boolean not null,
  "mbid" text not null,
  "copyright" text not null,
  "copyright_url" text not null,
  "release_id" bigint, total_disc INTEGER NOT NULL DEFAULT(0), total_track INTEGER NOT NULL DEFAULT(0), track_replay_gain REAL, release_replay_gain REAL, disc_subtitle TEXT NOT NULL DEFAULT '', recording_mbid TEXT,
  constraint "fk_track_release" foreign key ("release_id") references "release" ("id") on delete cascade deferrable initially deferred
))");
        session.getDboSession().execute("INSERT INTO track_backup SELECT id, version, scan_version, track_number, disc_number, name, duration, \"1900-01-01\", \"1900-01-01\", file_path, file_last_write, file_added, has_cover, mbid, copyright, copyright_url, release_id, total_disc, total_track, track_replay_gain, release_replay_gain, disc_subtitle, recording_mbid FROM track");
        session.getDboSession().execute("DROP TABLE track");
        session.getDboSession().execute("ALTER TABLE track_backup RENAME TO track");

        // Just increment the scan version of the settings to make the next scheduled scan rescan everything
        ScanSettings::get(session).modify()->incScanVersion();
    }

    static void migrateFromV31(Session& session)
    {
        // new star system, using dedicated entries per scrobbler and date time
        session.getDboSession().execute(R"(
CREATE TABLE "starred_artist" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "scrobbler" integer not null,
  "date_time" text,
  "artist_id" bigint,
  "user_id" bigint,
  constraint "fk_starred_artist_artist" foreign key ("artist_id") references "artist" ("id") on delete cascade deferrable initially deferred,
  constraint "fk_starred_artist_user" foreign key ("user_id") references "user" ("id") on delete cascade deferrable initially deferred
))");

        session.getDboSession().execute(R"(
CREATE TABLE "starred_release" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "scrobbler" integer not null,
  "date_time" text,
  "release_id" bigint,
  "user_id" bigint,
  constraint "fk_starred_release_release" foreign key ("release_id") references "release" ("id") on delete cascade deferrable initially deferred,
  constraint "fk_starred_release_user" foreign key ("user_id") references "user" ("id") on delete cascade deferrable initially deferred
))");

        session.getDboSession().execute(R"(
CREATE TABLE "starred_track" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "scrobbler" integer not null,
  "date_time" text,
  "track_id" bigint,
  "user_id" bigint,
  constraint "fk_starred_track_track" foreign key ("track_id") references "track" ("id") on delete cascade deferrable initially deferred,
  constraint "fk_starred_track_user" foreign key ("user_id") references "user" ("id") on delete cascade deferrable initially deferred
))");

        // Can't migrate using class mapping as mapping may evolve in the future

        // use time_t to avoid rounding issues later
        const std::string now{ dateTimeToDbFormat(Wt::WDateTime::fromTime_t(Wt::WDateTime::currentDateTime().toTime_t())) };

        std::map<IdType::ValueType, Scrobbler> userScrobblers;
        auto getScrobbler{ [&](IdType::ValueType userId)
        {
            auto itScrobbler {userScrobblers.find(userId)};
            if (itScrobbler != std::cend(userScrobblers))
                return itScrobbler->second;

            auto query {session.getDboSession().query<Scrobbler>("SELECT scrobbler FROM user WHERE id = ?").bind(userId)};
            [[maybe_unused]] auto [itInserted, inserted] {userScrobblers.emplace(userId, query.resultValue())};
            assert(inserted);
            return itInserted->second;
        } };

        auto migrateStarEntries{ [&session, &getScrobbler, &now](const std::string& colName, const std::string& oldTableName, const std::string& newTableName)
        {
            using UserIdObjectId = std::tuple<IdType::ValueType /* userId */, IdType::ValueType /* entryId */>;

            std::vector<UserIdObjectId> starredEntries;
            auto query {session.getDboSession().query<UserIdObjectId>("SELECT user_id, " + colName + " from " + oldTableName)};
            auto results {query.resultList()};

            LMS_LOG(DB, INFO) << "Found " << results.size() << " " << colName << " to migrate";

            for (const auto& [userId, entryId] : results)
            {
                session.getDboSession().execute("INSERT INTO " + newTableName + " ('version', 'scrobbler', 'date_time', '" + colName + "', 'user_id') VALUES (?, ?, ?, ?, ?)")
                    .bind(0)
                    .bind(getScrobbler(userId))
                    .bind(now)
                    .bind(entryId)
                    .bind(userId);
            }

            session.getDboSession().execute("DROP TABLE " + oldTableName);
        } };

        migrateStarEntries("artist_id", "user_artist_starred", "starred_artist");
        migrateStarEntries("release_id", "user_release_starred", "starred_release");
        migrateStarEntries("track_id", "user_track_starred", "starred_track");

        // new listen system, no longer using tracklists
        session.getDboSession().execute(R"(
CREATE TABLE "listen" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "date_time" text,
  "scrobbler" integer not null,
  "scrobbling_state" integer not null,
  "track_id" bigint,
  "user_id" bigint,
  constraint "fk_listen_track" foreign key ("track_id") references "track" ("id") on delete cascade deferrable initially deferred,
  constraint "fk_listen_user" foreign key ("user_id") references "user" ("id") on delete cascade deferrable initially deferred
))");

        auto migrateListens{ [&session](const std::string& trackListName, Scrobbler scrobbler)
        {
            using UserIdObjectId = std::tuple<IdType::ValueType /* userId */, IdType::ValueType /* trackId */, Wt::WDateTime>;

            std::vector<UserIdObjectId> listens;
            auto query {session.getDboSession().query<UserIdObjectId>("SELECT t_l.user_id, t_l_e.track_id, t_l_e.date_time FROM tracklist t_l")
                                                    .join("tracklist_entry t_l_e ON t_l_e.tracklist_id = t_l.id")
                                                    .where("t_l.name = ?").bind(trackListName)};
            auto results {query.resultList()};
            listens.reserve(results.size());

            LMS_LOG(DB, INFO) << "Found " << results.size() << " listens in " << trackListName;

            for (const auto& [userId, trackId, dateTime] : results)
            {
                session.getDboSession().execute("INSERT INTO listen ('version', 'date_time', 'scrobbler', 'scrobbling_state', 'track_id', 'user_id') VALUES (?, ?, ?, ?, ?, ?)")
                    .bind(0)
                    .bind(dateTimeToDbFormat(dateTime))
                    .bind(scrobbler)
                    .bind(ScrobblingState::Synchronized) // consider sync is done to avoid duplicate submissions
                    .bind(trackId)
                    .bind(userId);
            }
        } };

        migrateListens("__scrobbler_internal_history__", Scrobbler::Internal);
        migrateListens("__scrobbler_listenbrainz_history__", Scrobbler::ListenBrainz);
    }

    static void migrateFromV32(Session& session)
    {
        ScanSettings::get(session).modify()->addAudioFileExtension(".wv");
    }

    static void migrateFromV33(Session& session)
    {
        // remove name from track_artist_link
        // Drop Auth mode
        session.getDboSession().execute(R"(
CREATE TABLE IF NOT EXISTS "track_artist_link_backup" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "type" integer not null,
  "track_id" bigint,
  "artist_id" bigint,
  constraint "fk_track_artist_link_track" foreign key ("track_id") references "track" ("id") on delete cascade deferrable initially deferred,
  constraint "fk_track_artist_link_artist" foreign key ("artist_id") references "artist" ("id") on delete cascade deferrable initially deferred
);
))");
        session.getDboSession().execute("INSERT INTO track_artist_link_backup SELECT id, version, type, track_id, artist_id FROM track_artist_link");
        session.getDboSession().execute("DROP TABLE track_artist_link");
        session.getDboSession().execute("ALTER TABLE track_artist_link_backup RENAME TO track_artist_link");
    }

    static void migrateFromV34(Session& session)
    {
        // Add scrobbling state
        // By default, everything needs to be sent
        session.getDboSession().execute("ALTER TABLE starred_artist ADD scrobbling_state INTEGER NOT NULL DEFAULT(" + std::to_string(static_cast<int>(/*ScrobblingState::PendingAdd*/0)) + ")");
        session.getDboSession().execute("ALTER TABLE starred_release ADD scrobbling_state INTEGER NOT NULL DEFAULT(" + std::to_string(static_cast<int>(/*ScrobblingState::PendingAdd*/0)) + ")");
        session.getDboSession().execute("ALTER TABLE starred_track ADD scrobbling_state INTEGER NOT NULL DEFAULT(" + std::to_string(static_cast<int>(/*ScrobblingState::PendingAdd*/0)) + ")");
    }

    static void migrateFromV35(Session& session)
    {
        // Add creattion/last modif date time for tracklists
        session.getDboSession().execute("ALTER TABLE tracklist ADD creation_date_time TEXT");
        session.getDboSession().execute("ALTER TABLE tracklist ADD last_modified_date_time TEXT");
    }

    static void migrateFromV36(Session& session)
    {
        // Increased precision for track durations (now in milliseconds instead of secodns)
        // Just increment the scan version of the settings to make the next scheduled scan rescan everything
        ScanSettings::get(session).modify()->incScanVersion();
    }

    static void migrateFromV37(Session& session)
    {
        // Support Performer tags (via subtypes)
        session.getDboSession().execute("ALTER TABLE track_artist_link ADD subtype TEXT");

        // Just increment the scan version of the settings to make the next scheduled scan rescan everything
        ScanSettings::get(session).modify()->incScanVersion();
    }

    static void migrateFromV38(Session& session)
    {
        // migrate release-specific tags from Track to Release
        session.getDboSession().execute("ALTER TABLE release ADD total_disc INTEGER");

        session.getDboSession().execute(R"(
CREATE TABLE IF NOT EXISTS "track_backup" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "scan_version" integer not null,
  "track_number" integer,
  "disc_number" integer,
  "total_track" integer,
  "disc_subtitle" text not null,
  "name" text not null,
  "duration" integer,
  "date" text,
  "original_date" text,
  "file_path" text not null,
  "file_last_write" text,
  "file_added" text,
  "has_cover" boolean not null,
  "mbid" text not null,
  "recording_mbid" text not null,
  "copyright" text not null,
  "copyright_url" text not null,
  "track_replay_gain" real,
  "release_replay_gain" real,
  "release_id" bigint,
  constraint "fk_track_release" foreign key ("release_id") references "release" ("id") on delete cascade deferrable initially deferred
);
))");
        session.getDboSession().execute("INSERT INTO track_backup SELECT id, version, scan_version, track_number, disc_number, total_track, disc_subtitle, name, duration, date, original_date, file_path, file_last_write, file_added, has_cover, mbid, recording_mbid, copyright, copyright_url, track_replay_gain, release_replay_gain, release_id FROM track");
        session.getDboSession().execute("DROP TABLE track");
        session.getDboSession().execute("ALTER TABLE track_backup RENAME TO track");

        // Just increment the scan version of the settings to make the next scheduled scan rescan everything
        ScanSettings::get(session).modify()->incScanVersion();
    }

    static void migrateFromV39(Session& session)
    {
        // add release type
        session.getDboSession().execute("ALTER TABLE release ADD primary_type INTEGER");
        session.getDboSession().execute("ALTER TABLE release ADD secondary_types INTEGER");

        // Just increment the scan version of the settings to make the next scheduled scan rescan everything
        ScanSettings::get(session).modify()->incScanVersion();
    }

    static void migrateFromV40(Session& session)
    {
        // add artist_display_name in Release and Track
        session.getDboSession().execute("ALTER TABLE release ADD artist_display_name TEXT");
        session.getDboSession().execute("ALTER TABLE track ADD artist_display_name TEXT");

        // Just increment the scan version of the settings to make the next scheduled scan rescan everything
        ScanSettings::get(session).modify()->incScanVersion();
    }

    static void migrateFromV41(Session& session)
    {
        // add artist_display_name in Release and Track
        session.getDboSession().execute("ALTER TABLE user RENAME COLUMN subsonic_transcode_format TO subsonic_default_transcode_format");
        session.getDboSession().execute("ALTER TABLE user RENAME COLUMN subsonic_transcode_bitrate TO subsonic_default_transcode_bitrate");
        session.getDboSession().execute("ALTER TABLE user DROP COLUMN subsonic_transcode_enable");
    }

    void doDbMigration(Session& session)
    {
        static const std::string outdatedMsg{ "Outdated database, please rebuild it (delete the .db file and restart)" };

        ScopedNoForeignKeys noPragmaKeys{ session.getDb() };

        using MigrationFunction = std::function<void(Session&)>;

        const std::map<unsigned, MigrationFunction> migrationFunctions
        {
            {5, migrateFromV5},
            {6, migrateFromV6},
            {7, migrateFromV7},
            {8, migrateFromV8},
            {9, migrateFromV9},
            {10, migrateFromV10},
            {11, migrateFromV11},
            {12, migrateFromV12},
            {13, migrateFromV13},
            {14, migrateFromV14},
            {15, migrateFromV15},
            {16, migrateFromV16},
            {17, migrateFromV17},
            {18, migrateFromV18},
            {19, migrateFromV19},
            {20, migrateFromV20},
            {21, migrateFromV21},
            {22, migrateFromV22},
            {23, migrateFromV23},
            {24, migrateFromV24},
            {25, migrateFromV25},
            {26, migrateFromV26},
            {27, migrateFromV27},
            {28, migrateFromV28},
            {29, migrateFromV29},
            {30, migrateFromV30},
            {31, migrateFromV31},
            {32, migrateFromV32},
            {33, migrateFromV33},
            {34, migrateFromV34},
            {35, migrateFromV35},
            {36, migrateFromV36},
            {37, migrateFromV37},
            {38, migrateFromV38},
            {39, migrateFromV39},
            {40, migrateFromV40},
            {41, migrateFromV41},
        };

        {
            auto uniqueTransaction{ session.createUniqueTransaction() };

            Version version;
            try
            {
                version = VersionInfo::getOrCreate(session)->getVersion();
                LMS_LOG(DB, INFO) << "Database version = " << version << ", LMS binary version = " << LMS_DATABASE_VERSION;
            }
            catch (std::exception& e)
            {
                LMS_LOG(DB, ERROR) << "Cannot get database version info: " << e.what();
                throw LmsException{ outdatedMsg };
            }

            if (version > LMS_DATABASE_VERSION)
                throw LmsException{ "Server binary outdated, please upgrade it to handle this database" };

            if (version < migrationFunctions.begin()->first)
                throw LmsException{ outdatedMsg };

            while (version < LMS_DATABASE_VERSION)
            {
                LMS_LOG(DB, INFO) << "Migrating database from version " << version << " to " << version + 1 << "...";

                auto itMigrationFunc{ migrationFunctions.find(version) };
                assert(itMigrationFunc != std::cend(migrationFunctions));
                itMigrationFunc->second(session);

                VersionInfo::get(session).modify()->setVersion(++version);

                LMS_LOG(DB, INFO) << "Migration complete to version " << version;
            }
        }
    }
}
