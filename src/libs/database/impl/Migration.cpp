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

#include "core/Exception.hpp"
#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"
#include "database/Db.hpp"
#include "database/ScanSettings.hpp"
#include "database/Session.hpp"
#include "database/User.hpp"

#include "Utils.hpp"

namespace lms::db
{
    namespace
    {
        static constexpr Version LMS_DATABASE_VERSION{ 71 };
    }

    VersionInfo::VersionInfo()
        : _version{ LMS_DATABASE_VERSION }
    {
    }

    VersionInfo::pointer VersionInfo::getOrCreate(Session& session)
    {
        session.checkWriteTransaction();

        pointer versionInfo{ utils::fetchQuerySingleResult(session.getDboSession()->find<VersionInfo>()) };
        if (!versionInfo)
            return session.getDboSession()->add(std::make_unique<VersionInfo>());

        return versionInfo;
    }

    VersionInfo::pointer VersionInfo::get(Session& session)
    {
        session.checkReadTransaction();

        return utils::fetchQuerySingleResult(session.getDboSession()->find<VersionInfo>());
    }
} // namespace lms::db

namespace lms::db::Migration
{
    class ScopedNoForeignKeys
    {
    public:
        ScopedNoForeignKeys(Db& db)
            : _db{ db }
        {
            _db.executeSql("PRAGMA foreign_keys=OFF");
        }
        ~ScopedNoForeignKeys()
        {
            _db.executeSql("PRAGMA foreign_keys=ON");
        }

    private:
        ScopedNoForeignKeys(const ScopedNoForeignKeys&) = delete;
        ScopedNoForeignKeys(ScopedNoForeignKeys&&) = delete;
        ScopedNoForeignKeys& operator=(const ScopedNoForeignKeys&) = delete;
        ScopedNoForeignKeys& operator=(ScopedNoForeignKeys&&) = delete;

        Db& _db;
    };

    namespace
    {
        void migrateFromV33(Session& session)
        {
            // remove name from track_artist_link
            // Drop Auth mode
            utils::executeCommand(*session.getDboSession(), R"(
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
            utils::executeCommand(*session.getDboSession(), "INSERT INTO track_artist_link_backup SELECT id, version, type, track_id, artist_id FROM track_artist_link");
            utils::executeCommand(*session.getDboSession(), "DROP TABLE track_artist_link");
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE track_artist_link_backup RENAME TO track_artist_link");
        }

        void migrateFromV34(Session& session)
        {
            // Add scrobbling state
            // By default, everything needs to be sent
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE starred_artist ADD scrobbling_state INTEGER NOT NULL DEFAULT(" + std::to_string(static_cast<int>(/*ScrobblingState::PendingAdd*/ 0)) + ")");
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE starred_release ADD scrobbling_state INTEGER NOT NULL DEFAULT(" + std::to_string(static_cast<int>(/*ScrobblingState::PendingAdd*/ 0)) + ")");
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE starred_track ADD scrobbling_state INTEGER NOT NULL DEFAULT(" + std::to_string(static_cast<int>(/*ScrobblingState::PendingAdd*/ 0)) + ")");
        }

        void migrateFromV35(Session& session)
        {
            // Add creattion/last modif date time for tracklists
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE tracklist ADD creation_date_time TEXT");
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE tracklist ADD last_modified_date_time TEXT");
        }

        void migrateFromV36(Session& session)
        {
            // Increased precision for track durations (now in milliseconds instead of secodns)
            // Just increment the scan version of the settings to make the next scheduled scan rescan everything
            utils::executeCommand(*session.getDboSession(), "UPDATE scan_settings SET scan_version = scan_version + 1");
        }

        void migrateFromV37(Session& session)
        {
            // Support Performer tags (via subtypes)
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE track_artist_link ADD subtype TEXT");

            // Just increment the scan version of the settings to make the next scheduled scan rescan everything
            utils::executeCommand(*session.getDboSession(), "UPDATE scan_settings SET scan_version = scan_version + 1");
        }

        void migrateFromV38(Session& session)
        {
            // migrate release-specific tags from Track to Release
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE release ADD total_disc INTEGER");

            utils::executeCommand(*session.getDboSession(), R"(
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
            utils::executeCommand(*session.getDboSession(), "INSERT INTO track_backup SELECT id, version, scan_version, track_number, disc_number, total_track, disc_subtitle, name, duration, date, original_date, file_path, file_last_write, file_added, has_cover, mbid, recording_mbid, copyright, copyright_url, track_replay_gain, release_replay_gain, release_id FROM track");
            utils::executeCommand(*session.getDboSession(), "DROP TABLE track");
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE track_backup RENAME TO track");

            // Just increment the scan version of the settings to make the next scheduled scan rescan everything
            utils::executeCommand(*session.getDboSession(), "UPDATE scan_settings SET scan_version = scan_version + 1");
        }

        void migrateFromV39(Session& session)
        {
            // add release type
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE release ADD primary_type INTEGER");
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE release ADD secondary_types INTEGER");

            // Just increment the scan version of the settings to make the next scheduled scan rescan everything
            utils::executeCommand(*session.getDboSession(), "UPDATE scan_settings SET scan_version = scan_version + 1");
        }

        void migrateFromV40(Session& session)
        {
            // add artist_display_name in Release and Track
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE release ADD artist_display_name TEXT NOT NULL DEFAULT ''");
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE track ADD artist_display_name TEXT NOT NULL DEFAULT ''");

            // Just increment the scan version of the settings to make the next scheduled scan rescan everything
            utils::executeCommand(*session.getDboSession(), "UPDATE scan_settings SET scan_version = scan_version + 1");
        }

        void migrateFromV41(Session& session)
        {
            // add artist_display_name in Release and Track
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE user RENAME COLUMN subsonic_transcode_format TO subsonic_default_transcode_format");
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE user RENAME COLUMN subsonic_transcode_bitrate TO subsonic_default_transcode_bitrate");
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE user DROP COLUMN subsonic_transcode_enable");
        }

        void migrateFromV42(Session& session)
        {
            utils::executeCommand(*session.getDboSession(), "DROP INDEX IF EXISTS listen_scrobbler_idx");
            utils::executeCommand(*session.getDboSession(), "DROP INDEX IF EXISTS listen_user_scrobbler_idx");
            utils::executeCommand(*session.getDboSession(), "DROP INDEX IF EXISTS listen_user_track_scrobbler_date_time_idx");
            utils::executeCommand(*session.getDboSession(), "DROP INDEX IF EXISTS starred_artist_user_scrobbler_idx");
            utils::executeCommand(*session.getDboSession(), "DROP INDEX IF EXISTS starred_artist_artist_user_scrobbler_idx");
            utils::executeCommand(*session.getDboSession(), "DROP INDEX IF EXISTS starred_release_user_scrobbler_idx");
            utils::executeCommand(*session.getDboSession(), "DROP INDEX IF EXISTS starred_release_release_user_scrobbler_idx");
            utils::executeCommand(*session.getDboSession(), "DROP INDEX IF EXISTS starred_track_user_scrobbler_idx");
            utils::executeCommand(*session.getDboSession(), "DROP INDEX IF EXISTS starred_track_track_user_scrobbler_idx");

            // New feedback service that now handles the star/unstar stuff (that was previously handled by the scrobbling service)
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE user RENAME COLUMN scrobbler TO scrobbling_backend");
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE user ADD feedback_backend INTEGER");
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE listen RENAME COLUMN scrobbler TO backend");
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE listen RENAME COLUMN scrobbling_state TO sync_state");
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE starred_artist RENAME COLUMN scrobbler TO backend");
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE starred_artist RENAME COLUMN scrobbling_state TO sync_state");
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE starred_release RENAME COLUMN scrobbler TO backend");
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE starred_release RENAME COLUMN scrobbling_state TO sync_state");
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE starred_track RENAME COLUMN scrobbler TO backend");
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE starred_track RENAME COLUMN scrobbling_state TO sync_state");

            utils::executeCommand(*session.getDboSession(), "UPDATE user SET feedback_backend = scrobbling_backend");
        }

        void migrateFromV43(Session& session)
        {
            // add counts in genre table
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE cluster ADD track_count INTEGER");
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE cluster ADD release_count INTEGER");

            // Just increment the scan version of the settings to make the next scheduled scan rescan everything
            utils::executeCommand(*session.getDboSession(), "UPDATE scan_settings SET scan_version = scan_version + 1");
        }

        void migrateFromV44(Session& session)
        {
            // add bitrate
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE track ADD bitrate INTEGER NOT NULL DEFAULT 0");

            // Just increment the scan version of the settings to make the next scheduled scan rescan everything
            utils::executeCommand(*session.getDboSession(), "UPDATE scan_settings SET scan_version = scan_version + 1");
        }

        void migrateFromV45(Session& session)
        {
            // add subsonic_enable_transcoding_by_default, default is disabled
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE user ADD subsonic_enable_transcoding_by_default INTEGER NOT NULL DEFAULT(" + std::to_string(static_cast<int>(/*User::defaultSubsonicEnableTranscodingByDefault*/ 0)) + ")");
        }

        void migrateFromV46(Session& session)
        {
            // add extra tags to parse
            utils::executeCommand(*session.getDboSession(), R"(CREATE TABLE IF NOT EXISTS "cluster_type_backup" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "name" text not null
);)");
            utils::executeCommand(*session.getDboSession(), "INSERT INTO cluster_type_backup SELECT id, version, name FROM cluster_type");
            utils::executeCommand(*session.getDboSession(), "DROP TABLE cluster_type");
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE cluster_type_backup RENAME TO cluster_type");

            utils::executeCommand(*session.getDboSession(), "ALTER TABLE scan_settings ADD COLUMN extra_tags_to_scan TEXT");

            // Just increment the scan version of the settings to make the next scheduled scan rescan everything
            utils::executeCommand(*session.getDboSession(), "UPDATE scan_settings SET scan_version = scan_version + 1");
        }

        void migrateFromV47(Session& session)
        {
            // release type, new way
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE release DROP primary_type");
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE release DROP secondary_types");

            utils::executeCommand(*session.getDboSession(), R"(CREATE TABLE IF NOT EXISTS "release_type" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "name" text not null))");

            utils::executeCommand(*session.getDboSession(), R"(CREATE TABLE IF NOT EXISTS "release_release_type" (
  "release_type_id" bigint,
  "release_id" bigint,
  primary key ("release_type_id", "release_id"),
  constraint "fk_release_release_type_key1" foreign key ("release_type_id") references "release_type" ("id") on delete cascade deferrable initially deferred,
  constraint "fk_release_release_type_key2" foreign key ("release_id") references "release" ("id") on delete cascade deferrable initially deferred
))");
            utils::executeCommand(*session.getDboSession(), R"(CREATE INDEX "release_release_type_release_type" on "release_release_type" ("release_type_id"))");
            utils::executeCommand(*session.getDboSession(), R"(CREATE INDEX "release_release_type_release" on "release_release_type" ("release_id"))");

            // Just increment the scan version of the settings to make the next scheduled scan rescan everything
            utils::executeCommand(*session.getDboSession(), "UPDATE scan_settings SET scan_version = scan_version + 1");
        }

        void migrateFromV48(Session& session)
        {
            // Regression for the extra tags not being parsed
            // Just increment the scan version of the settings to make the next scheduled scan rescan everything
            utils::executeCommand(*session.getDboSession(), "UPDATE scan_settings SET scan_version = scan_version + 1");
        }

        void migrateFromV49(Session& session)
        {
            // Add year / originalYear fields, as date / originalDate are not enough (we don't want a wrong date but year or nothing)
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE track ADD year INTEGER");
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE track ADD original_year INTEGER");

            // Just increment the scan version of the settings to make the next scheduled scan rescan everything
            utils::executeCommand(*session.getDboSession(), "UPDATE scan_settings SET scan_version = scan_version + 1");
        }

        void migrateFromV50(Session& session)
        {
            // MediaLibrary support
            utils::executeCommand(*session.getDboSession(), R"(CREATE TABLE IF NOT EXISTS "media_library" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "path" text not null,
  "name" text not null
))");

            const int scanSettingsId{ session.getDboSession()->query<int>("SELECT id FROM scan_settings") };

            // Convert the existing media_directory in the scan_settings table to a media_library with id '1'
            utils::executeCommand(*session.getDboSession(), R"(INSERT INTO "media_library" ("id", "version", "path", "name")
SELECT 1, 0, s_s.media_directory, "Main"
FROM scan_settings s_s
WHERE id = ?)",
                scanSettingsId);

            // Remove the outdated column in scan_settings
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE scan_settings DROP media_directory");

            // Add the media_library column in tracks, with id '1'
            utils::executeCommand(*session.getDboSession(), R"(
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
  "bitrate" integer not null,
  "date" text,
  "year" integer,
  "original_date" text,
  "original_year" integer,
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
  "artist_display_name" text not null,
  "release_id" bigint,
  "media_library_id" bigint,
  constraint "fk_track_release" foreign key ("release_id") references "release" ("id") on delete cascade deferrable initially deferred,
  constraint "fk_track_media_library" foreign key ("media_library_id") references "media_library" ("id") on delete set null deferrable initially deferred
))");

            // Migrate data, with the new media_library_id field set to 1
            utils::executeCommand(*session.getDboSession(), R"(INSERT INTO track_backup 
SELECT
 id,
 version,
 scan_version,
 track_number,
 disc_number,
 total_track,
 disc_subtitle,
 name,
 duration,
 COALESCE(bitrate, 0),
 date,
 year,
 original_date,
 original_year,
 file_path,
 file_last_write,
 file_added,
 has_cover,
 mbid,
 recording_mbid,
 copyright,
 copyright_url,
 track_replay_gain,
 release_replay_gain,
 COALESCE(artist_display_name, ""),
 release_id,
 1
 FROM track)");
            utils::executeCommand(*session.getDboSession(), "DROP TABLE track");
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE track_backup RENAME TO track");
        }

        void migrateFromV51(Session& session)
        {
            // Add custom artist tag delimiters, no need to rescan since it has no effect when empty
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE scan_settings ADD artist_tag_delimiters TEXT NOT NULL DEFAULT ''");
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE scan_settings ADD default_tag_delimiters TEXT NOT NULL DEFAULT ''");
        }

        void migrateFromV52(Session& session)
        {
            // Add sort name for releases
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE release ADD sort_name TEXT NOT NULL DEFAULT ''");

            // Just increment the scan version of the settings to make the next scheduled scan rescan everything
            utils::executeCommand(*session.getDboSession(), "UPDATE scan_settings SET scan_version = scan_version + 1");
        }

        void migrateFromV53(Session& session)
        {
            // Add release group mbid
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE release ADD group_mbid TEXT NOT NULL DEFAULT ''");

            // Just increment the scan version of the settings to make the next scheduled scan rescan everything
            utils::executeCommand(*session.getDboSession(), "UPDATE scan_settings SET scan_version = scan_version + 1");
        }

        void migrateFromV54(Session& session)
        {
            // Add file size + relative file path
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE track RENAME COLUMN file_path TO absolute_file_path");
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE track ADD file_size BIGINT NOT NULL DEFAULT(0)");
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE track ADD relative_file_path TEXT NOT NULL DEFAULT ''");

            // Just increment the scan version of the settings to make the next scheduled scan rescan everything
            utils::executeCommand(*session.getDboSession(), "UPDATE scan_settings SET scan_version = scan_version + 1");
        }

        void migrateFromV55(Session& session)
        {
            // Add bitsPerSample, channelCount and sampleRate
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE track ADD bits_per_sample INTEGER NOT NULL DEFAULT(0)");
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE track ADD channel_count INTEGER NOT NULL DEFAULT(0)");
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE track ADD sample_rate INTEGER NOT NULL DEFAULT(0)");

            // Just increment the scan version of the settings to make the next scheduled scan rescan everything
            utils::executeCommand(*session.getDboSession(), "UPDATE scan_settings SET scan_version = scan_version + 1");
        }

        void migrateFromV56(Session& session)
        {
            // Make sure we remove all the previoulsy created index, the createIndexesIfNeeded will recreate them all
            std::vector<std::string> indexeNames{ utils::fetchQueryResults(session.getDboSession()->query<std::string>(R"(SELECT name FROM sqlite_master WHERE type = 'index' AND name LIKE '%_idx')")) };
            for (const auto& indexName : indexeNames)
                utils::executeCommand(*session.getDboSession(), "DROP INDEX " + indexName);
        }

        void migrateFromV57(Session& session)
        {
            // useless index, may have been already removed in the previous step
            utils::executeCommand(*session.getDboSession(), "DROP INDEX IF EXISTS cluster_name_idx");
        }

        void migrateFromV58(Session& session)
        {
            // DSF support
            utils::executeCommand(*session.getDboSession(), "UPDATE scan_settings SET audio_file_extensions = audio_file_extensions || ' .dsf'");
        }

        void migrateFromV59(Session& session)
        {
            // Dedicated image table
            utils::executeCommand(*session.getDboSession(), R"(CREATE TABLE IF NOT EXISTS "image" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "path" text not null,
  "stem" text not null,
  "file_last_write" text,
  "file_size" integer not null,
  "width" integer not null,
  "height" integer not null,
  "artist_id" bigint,
  constraint "fk_image_artist" foreign key ("artist_id") references "artist" ("id") on delete cascade deferrable initially deferred
))");

            // Just increment the scan version of the settings to make the next scheduled scan rescan everything
            utils::executeCommand(*session.getDboSession(), "UPDATE scan_settings SET scan_version = scan_version + 1");
        }

        void migrateFromV60(Session& session)
        {
            // Dedicated directory table
            utils::executeCommand(*session.getDboSession(), R"(CREATE TABLE IF NOT EXISTS "directory" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "absolute_path" text not null,
  "name" text not null,
  "parent_directory_id" bigint,
  constraint "fk_directory_directory" foreign key ("parent_directory_id") references "directory" ("id") on delete cascade deferrable initially deferred
))");

            // Add a ref in track, need to recreate a new table
            utils::executeCommand(*session.getDboSession(), R"(
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
  "bitrate" integer not null,
  "bits_per_sample" integer not null,
  "channel_count" integer not null,
  "sample_rate" integer not null,
  "date" text,
  "year" integer,
  "original_date" text,
  "original_year" integer,
  "absolute_file_path" text not null,
  "relative_file_path" text not null,
  "file_size" bigint not null,
  "file_last_write" text,
  "file_added" text,
  "has_cover" boolean not null,
  "mbid" text not null,
  "recording_mbid" text not null,
  "copyright" text not null,
  "copyright_url" text not null,
  "track_replay_gain" real,
  "release_replay_gain" real,
  "artist_display_name" text not null,
  "release_id" bigint,
  "media_library_id" bigint,
  "directory_id" bigint,
  constraint "fk_track_release" foreign key ("release_id") references "release" ("id") on delete cascade deferrable initially deferred,
  constraint "fk_track_media_library" foreign key ("media_library_id") references "media_library" ("id") on delete set null deferrable initially deferred,
  constraint "fk_track_directory" foreign key ("directory_id") references "directory" ("id") on delete cascade deferrable initially deferred
))");
            // Migrate data, with the new directory_id field set to null
            utils::executeCommand(*session.getDboSession(), R"(INSERT INTO track_backup 
SELECT
 id,
 version,
 scan_version,
 track_number,
 disc_number,
 total_track,
 disc_subtitle,
 name,
 duration,
 bitrate,
 bits_per_sample,
 channel_count,
 sample_rate,
 date,
 year,
 original_date,
 original_year,
 absolute_file_path,
 relative_file_path,
 file_size,
 file_last_write,
 file_added,
 has_cover,
 mbid,
 recording_mbid,
 copyright,
 copyright_url,
 track_replay_gain,
 release_replay_gain,
 artist_display_name,
 release_id,
 media_library_id,
 NULL
 FROM track)");
            utils::executeCommand(*session.getDboSession(), "DROP TABLE track");
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE track_backup RENAME TO track");

            // Add a ref in image + rename path to absolute_file_path, need to recreate a new table
            utils::executeCommand(*session.getDboSession(), R"(
            CREATE TABLE IF NOT EXISTS "image_backup" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "absolute_file_path" text not null,
  "stem" text not null,
  "file_last_write" text,
  "file_size" integer not null,
  "width" integer not null,
  "height" integer not null,
  "artist_id" bigint,
  "directory_id" bigint,
  constraint "fk_image_artist" foreign key ("artist_id") references "artist" ("id") on delete cascade deferrable initially deferred,
  constraint "fk_image_directory" foreign key ("directory_id") references "directory" ("id") on delete cascade deferrable initially deferred
))");

            // Migrate data, with the new directory_id field set to null
            utils::executeCommand(*session.getDboSession(), R"(INSERT INTO image_backup 
SELECT
 id,
 version,
 path,
 stem,
 file_last_write,
 file_size,
 width,
 height,
 artist_id,
 NULL
 FROM image
 )");
            utils::executeCommand(*session.getDboSession(), "DROP TABLE image");
            utils::executeCommand(*session.getDboSession(), "ALTER TABLE image_backup RENAME TO image");

            // Just increment the scan version of the settings to make the next scheduled scan rescan everything
            utils::executeCommand(*session.getDboSession(), "UPDATE scan_settings SET scan_version = scan_version + 1");
        }

    } // namespace

    void migrateFromV61(Session& session)
    {
        // Added a media_library_id in Directory
        utils::executeCommand(*session.getDboSession(), R"(
CREATE TABLE IF NOT EXISTS "directory_backup" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "absolute_path" text not null,
  "name" text not null,
  "parent_directory_id" bigint,
  "media_library_id" bigint,
  constraint "fk_directory_parent_directory" foreign key ("parent_directory_id") references "directory" ("id") on delete cascade deferrable initially deferred,
  constraint "fk_directory_media_library" foreign key ("media_library_id") references "media_library" ("id") on delete set null deferrable initially deferred
  ))");

        // Migrate data, with the new directory_id field set to null
        utils::executeCommand(*session.getDboSession(), R"(INSERT INTO directory_backup 
SELECT
 id,
 version,
 absolute_path,
 name,
 parent_directory_id,
 NULL
 FROM directory)");

        utils::executeCommand(*session.getDboSession(), "DROP TABLE directory");
        utils::executeCommand(*session.getDboSession(), "ALTER TABLE directory_backup RENAME TO directory");

        // Just increment the scan version of the settings to make the next scheduled scan rescan everything
        utils::executeCommand(*session.getDboSession(), "UPDATE scan_settings SET scan_version = scan_version + 1");
    }

    void migrateFromV62(Session& session)
    {
        // Add a new column comment
        utils::executeCommand(*session.getDboSession(), "ALTER TABLE track ADD comment TEXT NOT NULL DEFAULT ''");

        // Just increment the scan version of the settings to make the next scheduled scan rescan everything
        utils::executeCommand(*session.getDboSession(), "UPDATE scan_settings SET scan_version = scan_version + 1");
    }

    void migrateFromV63(Session& session)
    {
        // Add a rated entities

        utils::executeCommand(*session.getDboSession(), R"(CREATE TABLE IF NOT EXISTS "rated_artist" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "rating" integer not null,
  "last_updated" text,
  "artist_id" bigint,
  "user_id" bigint,
  constraint "fk_rated_artist_artist" foreign key ("artist_id") references "artist" ("id") on delete cascade deferrable initially deferred,
  constraint "fk_rated_artist_user" foreign key ("user_id") references "user" ("id") on delete cascade deferrable initially deferred
))");

        utils::executeCommand(*session.getDboSession(), R"(CREATE TABLE IF NOT EXISTS "rated_release" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "rating" integer not null,
  "last_updated" text,
  "release_id" bigint,
  "user_id" bigint,
  constraint "fk_rated_release_release" foreign key ("release_id") references "release" ("id") on delete cascade deferrable initially deferred,
  constraint "fk_rated_release_user" foreign key ("user_id") references "user" ("id") on delete cascade deferrable initially deferred
))");

        utils::executeCommand(*session.getDboSession(), R"(CREATE TABLE IF NOT EXISTS "rated_track" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "rating" bigint not null,
  "last_updated" text,
  "track_id" bigint,
  "user_id" bigint,
  constraint "fk_rated_track_track" foreign key ("track_id") references "track" ("id") on delete cascade deferrable initially deferred,
  constraint "fk_rated_track_user" foreign key ("user_id") references "user" ("id") on delete cascade deferrable initially deferred
))");

        // Drop badly named index, will be recreated
        utils::executeCommand(*session.getDboSession(), "DROP INDEX IF EXISTS listen_user_backend_date_time");
    }

    void migrateFromV64(Session& session)
    {
        utils::executeCommand(*session.getDboSession(), R"(CREATE TABLE IF NOT EXISTS "label" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "name" text not null
))");

        utils::executeCommand(*session.getDboSession(), R"(CREATE TABLE IF NOT EXISTS "release_label" (
  "label_id" bigint,
  "release_id" bigint,
  primary key ("label_id", "release_id"),
  constraint "fk_release_label_key1" foreign key ("label_id") references "label" ("id") on delete cascade deferrable initially deferred,
  constraint "fk_release_label_key2" foreign key ("release_id") references "release" ("id") on delete cascade deferrable initially deferred
))");
        utils::executeCommand(*session.getDboSession(), R"(CREATE INDEX "release_label_label" on "release_label" ("label_id"))");
        utils::executeCommand(*session.getDboSession(), R"(CREATE INDEX "release_label_release" on "release_label" ("release_id"))");

        // Just increment the scan version of the settings to make the next scheduled scan rescan everything
        utils::executeCommand(*session.getDboSession(), "UPDATE scan_settings SET scan_version = scan_version + 1");
    }

    void migrateFromV65(Session& session)
    {
        utils::executeCommand(*session.getDboSession(), "ALTER TABLE release ADD is_compilation BOOLEAN NOT NULL DEFAULT(false)");

        // Just increment the scan version of the settings to make the next scheduled scan rescan everything
        utils::executeCommand(*session.getDboSession(), "UPDATE scan_settings SET scan_version = scan_version + 1");
    }

    void migrateFromV66(Session& session)
    {
        // New way of handling UI settings
        utils::executeCommand(*session.getDboSession(), R"(CREATE TABLE IF NOT EXISTS "ui_state" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "item" text not null,
  "value" text not null,
  "user_id" bigint,
  constraint "fk_ui_state_user" foreign key ("user_id") references "user" ("id") on delete cascade deferrable initially deferred
))");

        utils::executeCommand(*session.getDboSession(), "ALTER TABLE user DROP COLUMN repeat_all");
        utils::executeCommand(*session.getDboSession(), "ALTER TABLE user DROP COLUMN radio");
        utils::executeCommand(*session.getDboSession(), "ALTER TABLE user DROP COLUMN cur_playing_track_pos");
    }

    void migrateFromV67(Session& session)
    {
        // Add a ref to release in image
        utils::executeCommand(*session.getDboSession(), R"(CREATE TABLE "image_backup" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "absolute_file_path" text not null,
  "stem" text not null,
  "file_last_write" text,
  "file_size" integer not null,
  "width" integer not null,
  "height" integer not null,
  "artist_id" bigint,
  "release_id" bigint,
  "directory_id" bigint,
  constraint "fk_image_artist" foreign key ("artist_id") references "artist" ("id") on delete cascade deferrable initially deferred,
  constraint "fk_image_release" foreign key ("release_id") references "release" ("id") on delete cascade deferrable initially deferred,
  constraint "fk_image_directory" foreign key ("directory_id") references "directory" ("id") on delete cascade deferrable initially deferred
))");

        // Migrate data, with the new release_id field set to null
        utils::executeCommand(*session.getDboSession(), R"(INSERT INTO image_backup 
SELECT
 id,
 version,
 absolute_file_path,
 stem,
 file_last_write,
 file_size,
 width,
 height,
 artist_id,
 NULL,
 directory_id
 FROM image
 )");
        utils::executeCommand(*session.getDboSession(), "DROP TABLE image");
        utils::executeCommand(*session.getDboSession(), "ALTER TABLE image_backup RENAME TO image");

        // Changed some indexes for the image table -> remove all the previoulsy created index, the createIndexesIfNeeded will recreate them all
        std::vector<std::string> indexeNames{ utils::fetchQueryResults(session.getDboSession()->query<std::string>(R"(SELECT name FROM sqlite_master WHERE type = 'index' AND name LIKE '%_idx')")) };
        for (const auto& indexName : indexeNames)
            utils::executeCommand(*session.getDboSession(), "DROP INDEX " + indexName);

        // Just increment the scan version of the settings to make the next scheduled scan rescan everything
        utils::executeCommand(*session.getDboSession(), "UPDATE scan_settings SET scan_version = scan_version + 1");
    }

    void migrateFromV68(Session& session)
    {
        // Changed the way we ref images from release and artists (several releases and artist can now share the same image)
        utils::executeCommand(*session.getDboSession(), R"(CREATE TABLE "release_backup" (
"id" integer primary key autoincrement,
"version" integer not null,
"name" text not null,
"sort_name" text not null,
"mbid" text not null,
"group_mbid" text not null,
"total_disc" integer,
"artist_display_name" text not null,
"is_compilation" boolean not null,
"image_id" bigint,
constraint "fk_release_image" foreign key ("image_id") references "image" ("id") on delete set null deferrable initially deferred))");

        // Migrate data, with the new image_id field set to null
        utils::executeCommand(*session.getDboSession(), R"(INSERT INTO release_backup 
SELECT
 id,
 version,
 name,
 sort_name,
 mbid,
 group_mbid,
 total_disc,
 COALESCE(artist_display_name, ""),
 is_compilation,
 NULL
 FROM release
 )");
        utils::executeCommand(*session.getDboSession(), "DROP TABLE release");
        utils::executeCommand(*session.getDboSession(), "ALTER TABLE release_backup RENAME TO release");

        utils::executeCommand(*session.getDboSession(), R"(CREATE TABLE IF NOT EXISTS "artist_backup" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "name" text not null,
  "sort_name" text not null,
  "mbid" text not null,
  "image_id" bigint,
  constraint "fk_artist_image" foreign key ("image_id") references "image" ("id") on delete set null deferrable initially deferred
))");

        // Migrate data, with the new image_id field set to null
        utils::executeCommand(*session.getDboSession(), R"(INSERT INTO artist_backup 
SELECT
 id,
 version,
 name,
 sort_name,
 mbid,
 NULL
 FROM artist
 )");

        utils::executeCommand(*session.getDboSession(), "DROP TABLE artist");
        utils::executeCommand(*session.getDboSession(), "ALTER TABLE artist_backup RENAME TO artist");

        // Just increment the scan version of the settings to make the next scheduled scan rescan everything
        utils::executeCommand(*session.getDboSession(), "UPDATE scan_settings SET scan_version = scan_version + 1");
    }

    void migrateFromV69(Session& session)
    {
        // Add a field in UI settings
        utils::executeCommand(*session.getDboSession(), "ALTER TABLE user ADD COLUMN ui_artist_release_sort_method NOT NULL DEFAULT 7"); // 7 = ReleaseSortMethod::OriginalDateDesc
    }

    void migrateFromV70(Session& session)
    {
        // Add a file name/stem in tracks
        utils::executeCommand(*session.getDboSession(), "ALTER TABLE track ADD COLUMN file_stem TEXT NOT NULL DEFAULT ''");

        // New table TrackLyrics
        utils::executeCommand(*session.getDboSession(), R"(CREATE TABLE IF NOT EXISTS "track_lyrics" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "absolute_file_path" text not null,
  "stem" text not null,
  "file_last_write" text,
  "file_size" integer not null,
  "lines" text not null,
  "language" text not null,
  "offset" integer,
  "display_artist" text not null,
  "display_title" text not null,
  "synchronized" boolean not null,
  "track_id" bigint,
  "directory_id" bigint,
  constraint "fk_track_lyrics_track" foreign key ("track_id") references "track" ("id") on delete cascade deferrable initially deferred,
  constraint "fk_track_lyrics_directory" foreign key ("directory_id") references "directory" ("id") on delete cascade deferrable initially deferred
))");

        // Just increment the scan version of the settings to make the next scheduled scan rescan everything
        utils::executeCommand(*session.getDboSession(), "UPDATE scan_settings SET scan_version = scan_version + 1");
    }

    bool doDbMigration(Session& session)
    {
        constexpr std::string_view outdatedMsg{ "Outdated database, please rebuild it (delete the .db file and restart)" };

        ScopedNoForeignKeys noPragmaKeys{ session.getDb() };

        using MigrationFunction = std::function<void(Session&)>;

        const std::map<unsigned, MigrationFunction> migrationFunctions{
            { 33, migrateFromV33 },
            { 34, migrateFromV34 },
            { 35, migrateFromV35 },
            { 36, migrateFromV36 },
            { 37, migrateFromV37 },
            { 38, migrateFromV38 },
            { 39, migrateFromV39 },
            { 40, migrateFromV40 },
            { 41, migrateFromV41 },
            { 42, migrateFromV42 },
            { 43, migrateFromV43 },
            { 44, migrateFromV44 },
            { 45, migrateFromV45 },
            { 46, migrateFromV46 },
            { 47, migrateFromV47 },
            { 48, migrateFromV48 },
            { 49, migrateFromV49 },
            { 50, migrateFromV50 },
            { 51, migrateFromV51 },
            { 52, migrateFromV52 },
            { 53, migrateFromV53 },
            { 54, migrateFromV54 },
            { 55, migrateFromV55 },
            { 56, migrateFromV56 },
            { 57, migrateFromV57 },
            { 58, migrateFromV58 },
            { 59, migrateFromV59 },
            { 60, migrateFromV60 },
            { 61, migrateFromV61 },
            { 62, migrateFromV62 },
            { 63, migrateFromV63 },
            { 64, migrateFromV64 },
            { 65, migrateFromV65 },
            { 66, migrateFromV66 },
            { 67, migrateFromV67 },
            { 68, migrateFromV68 },
            { 69, migrateFromV69 },
            { 70, migrateFromV70 },
        };

        bool migrationPerformed{};
        {
            LMS_SCOPED_TRACE_OVERVIEW("Database", "Migration");
            auto transaction{ session.createWriteTransaction() };

            Version version;
            try
            {
                version = VersionInfo::getOrCreate(session)->getVersion();
                LMS_LOG(DB, INFO, "Database version = " << version << ", LMS binary version = " << LMS_DATABASE_VERSION);
            }
            catch (std::exception& e)
            {
                LMS_LOG(DB, ERROR, "Cannot get database version info: " << e.what());
                throw core::LmsException{ outdatedMsg };
            }

            if (version > LMS_DATABASE_VERSION)
                throw core::LmsException{ "Server binary outdated, please upgrade it to handle this database" };

            if (version < migrationFunctions.begin()->first)
                throw core::LmsException{ outdatedMsg };

            while (version < LMS_DATABASE_VERSION)
            {
                LMS_SCOPED_TRACE_DETAILED("Database", "MigrationStep");
                LMS_LOG(DB, INFO, "Migrating database from version " << version << " to " << version + 1 << "...");

                auto itMigrationFunc{ migrationFunctions.find(version) };
                assert(itMigrationFunc != std::cend(migrationFunctions));
                itMigrationFunc->second(session);

                VersionInfo::get(session).modify()->setVersion(++version);

                LMS_LOG(DB, INFO, "Migration complete to version " << version);
                migrationPerformed = true;
            }
        }

        return migrationPerformed;
    }
} // namespace lms::db::Migration
