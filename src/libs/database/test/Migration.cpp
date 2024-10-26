/*
 * Copyright (C) 2021 Emeric Poupon
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

#include "Common.hpp"

#include "core/String.hpp"
#include "database/Db.hpp"
#include "database/Directory.hpp"
#include "database/Image.hpp"
#include "database/RatedArtist.hpp"
#include "database/RatedRelease.hpp"
#include "database/RatedTrack.hpp"
#include "database/StarredArtist.hpp"
#include "database/StarredRelease.hpp"
#include "database/StarredTrack.hpp"
#include "database/TrackLyrics.hpp"
#include "database/UIState.hpp"
#include "database/User.hpp"

namespace lms::db::tests
{
    namespace
    {
        void executeStatements(db::Session& session, std::string_view statements)
        {
            for (std::string_view statement : core::stringUtils::splitString(statements, ';'))
            {
                statement = core::stringUtils::stringTrim(statement, " \t\r\n");
                if (!statement.empty())
                    session.execute(statement);
            }
        }
    } // namespace

    TEST(Database, migration)
    {
        TmpDatabase tmpDatabase;

        auto& db{ tmpDatabase.getDb() };

        // Create tables and indexes as they were in version 33:
        const std::string_view schemaV33{ R"(
CREATE TABLE IF NOT EXISTS "cluster_type" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "name" text not null,
  "scan_settings_id" bigint,
  constraint "fk_cluster_type_scan_settings" foreign key ("scan_settings_id") references "scan_settings" ("id") on delete cascade deferrable initially deferred
);
CREATE TABLE IF NOT EXISTS "version_info" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "db_version" integer not null
);
CREATE TABLE IF NOT EXISTS "scan_settings" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "scan_version" integer not null,
  "media_directory" text not null,
  "start_time" integer,
  "update_period" integer not null,
  "audio_file_extensions" text not null,
  "similarity_engine_type" integer not null
);
CREATE TABLE IF NOT EXISTS "track_features" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "data" text not null,
  "track_id" bigint,
  constraint "fk_track_features_track" foreign key ("track_id") references "track" ("id") on delete cascade deferrable initially deferred
);
CREATE TABLE IF NOT EXISTS "tracklist_entry" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "track_id" bigint,
  "tracklist_id" bigint, date_time TEXT,
  constraint "fk_tracklist_entry_track" foreign key ("track_id") references "track" ("id") on delete cascade deferrable initially deferred,
  constraint "fk_tracklist_entry_tracklist" foreign key ("tracklist_id") references "tracklist" ("id") on delete cascade deferrable initially deferred
);
CREATE TABLE IF NOT EXISTS "track_artist_link" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "type" integer not null,
  "name" integer not null,
  "track_id" bigint,
  "artist_id" bigint,
  constraint "fk_track_artist_link_track" foreign key ("track_id") references "track" ("id") on delete cascade deferrable initially deferred,
  constraint "fk_track_artist_link_artist" foreign key ("artist_id") references "artist" ("id") on delete cascade deferrable initially deferred
);
CREATE TABLE IF NOT EXISTS "artist" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "name" text not null,
  "sort_name" text not null,
  "mbid" text not null
);
CREATE TABLE IF NOT EXISTS "cluster" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "name" text not null,
  "cluster_type_id" bigint,
  constraint "fk_cluster_cluster_type" foreign key ("cluster_type_id") references "cluster_type" ("id") on delete cascade deferrable initially deferred
);
CREATE TABLE IF NOT EXISTS "auth_token" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "value" text not null,
  "expiry" text,
  "user_id" bigint,
  constraint "fk_auth_token_user" foreign key ("user_id") references "user" ("id") on delete cascade deferrable initially deferred
);
CREATE TABLE IF NOT EXISTS "tracklist" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "name" text not null,
  "type" integer not null,
  "public" boolean not null,
  "user_id" bigint,
  constraint "fk_tracklist_user" foreign key ("user_id") references "user" ("id") on delete cascade deferrable initially deferred
);
CREATE TABLE IF NOT EXISTS "track_cluster" (
  "track_id" bigint,
  "cluster_id" bigint,
  primary key ("track_id", "cluster_id"),
  constraint "fk_track_cluster_key1" foreign key ("track_id") references "track" ("id") on delete cascade deferrable initially deferred,
  constraint "fk_track_cluster_key2" foreign key ("cluster_id") references "cluster" ("id") on delete cascade deferrable initially deferred
);
CREATE TABLE IF NOT EXISTS "track_bookmark" (
        "id" integer primary key autoincrement,
        "version" integer not null,
        "offset" integer,
        "comment" text not null,
        "track_id" bigint,
        "user_id" bigint,
        constraint "fk_track_bookmark_track" foreign key ("track_id") references "track" ("id") on delete cascade deferrable initially deferred,
        constraint "fk_track_bookmark_user" foreign key ("user_id") references "user" ("id") on delete cascade deferrable initially deferred
);
CREATE TABLE IF NOT EXISTS "release" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "name" text not null,
  "mbid" text not null
);
CREATE INDEX "track_cluster_track" on "track_cluster" ("track_id");
CREATE INDEX "track_cluster_cluster" on "track_cluster" ("cluster_id");
CREATE INDEX artist_name_idx ON artist(name);
CREATE INDEX artist_sort_name_nocase_idx ON artist(sort_name COLLATE NOCASE);
CREATE INDEX artist_mbid_idx ON artist(mbid);
CREATE INDEX auth_token_user_idx ON auth_token(user_id);
CREATE INDEX auth_token_expiry_idx ON auth_token(expiry);
CREATE INDEX auth_token_value_idx ON auth_token(value);
CREATE INDEX cluster_name_idx ON cluster(name);
CREATE INDEX cluster_cluster_type_idx ON cluster(cluster_type_id);
CREATE INDEX cluster_type_name_idx ON cluster_type(name);
CREATE INDEX tracklist_name_idx ON tracklist(name);
CREATE INDEX tracklist_user_idx ON tracklist(user_id);
CREATE INDEX track_features_track_idx ON track_features(track_id);
CREATE INDEX track_artist_link_artist_idx ON track_artist_link(artist_id);
CREATE INDEX track_artist_link_name_idx ON track_artist_link(name);
CREATE INDEX track_artist_link_track_idx ON track_artist_link(track_id);
CREATE INDEX track_artist_link_type_idx ON track_artist_link(type);
CREATE INDEX track_bookmark_track_idx ON track_bookmark(track_id);
CREATE INDEX track_bookmark_user_idx ON track_bookmark(user_id);
CREATE INDEX track_bookmark_user_track_idx ON track_bookmark(user_id,track_id);
CREATE INDEX release_name_idx ON release(name);
CREATE INDEX release_name_nocase_idx ON release(name COLLATE NOCASE);
CREATE INDEX release_mbid_idx ON release(mbid);
CREATE INDEX tracklist_entry_track_idx ON tracklist_entry(track_id);
CREATE TABLE IF NOT EXISTS "user" (
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
, listenbrainz_token TEXT, scrobbler INTEGER NOT NULL DEFAULT(0));
CREATE TABLE IF NOT EXISTS "track" (
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
);
CREATE INDEX track_file_last_write_idx ON track(file_last_write);
CREATE INDEX track_path_idx ON track(file_path);
CREATE INDEX track_name_idx ON track(name);
CREATE INDEX track_name_nocase_idx ON track(name COLLATE NOCASE);
CREATE INDEX track_mbid_idx ON track(mbid);
CREATE INDEX track_recording_mbid_idx ON track(recording_mbid);
CREATE INDEX track_release_idx ON track(release_id);
CREATE INDEX track_date_idx ON track(date);
CREATE INDEX track_original_date_idx ON track(original_date);
CREATE TABLE IF NOT EXISTS "starred_artist" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "scrobbler" integer not null,
  "date_time" text,
  "artist_id" bigint,
  "user_id" bigint,
  constraint "fk_starred_artist_artist" foreign key ("artist_id") references "artist" ("id") on delete cascade deferrable initially deferred,
  constraint "fk_starred_artist_user" foreign key ("user_id") references "user" ("id") on delete cascade deferrable initially deferred
);
CREATE TABLE IF NOT EXISTS "starred_release" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "scrobbler" integer not null,
  "date_time" text,
  "release_id" bigint,
  "user_id" bigint,
  constraint "fk_starred_release_release" foreign key ("release_id") references "release" ("id") on delete cascade deferrable initially deferred,
  constraint "fk_starred_release_user" foreign key ("user_id") references "user" ("id") on delete cascade deferrable initially deferred
);
CREATE TABLE IF NOT EXISTS "starred_track" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "scrobbler" integer not null,
  "date_time" text,
  "track_id" bigint,
  "user_id" bigint,
  constraint "fk_starred_track_track" foreign key ("track_id") references "track" ("id") on delete cascade deferrable initially deferred,
  constraint "fk_starred_track_user" foreign key ("user_id") references "user" ("id") on delete cascade deferrable initially deferred
);
CREATE TABLE IF NOT EXISTS "listen" (
  "id" integer primary key autoincrement,
  "version" integer not null,
  "date_time" text,
  "scrobbler" integer not null,
  "scrobbling_state" integer not null,
  "track_id" bigint,
  "user_id" bigint,
  constraint "fk_listen_track" foreign key ("track_id") references "track" ("id") on delete cascade deferrable initially deferred,
  constraint "fk_listen_user" foreign key ("user_id") references "user" ("id") on delete cascade deferrable initially deferred
);
CREATE INDEX listen_scrobbler_idx ON listen(scrobbler);
CREATE INDEX listen_user_scrobbler_idx ON listen(user_id,scrobbler);
CREATE INDEX starred_artist_user_scrobbler_idx ON starred_artist(user_id,scrobbler);
CREATE INDEX starred_release_user_scrobbler_idx ON starred_release(user_id,scrobbler);
CREATE INDEX starred_track_user_scrobbler_idx ON starred_track(user_id,scrobbler);)" };

        const std::string_view createDummyData{ R"(
-- Inserting artists
INSERT INTO artist (version, name, sort_name, mbid) VALUES
(1, 'Artist A', 'Artist A', 'mbid_artist_a'),
(2, 'Artist B', 'Artist B', 'mbid_artist_b');

-- Inserting releases
INSERT INTO release (version, name, mbid) VALUES
(1, 'Release X', 'mbid_release_x'),
(2, 'Release Y', 'mbid_release_y');

-- Inserting tracks without any associated artists or releases (Orphan Tracks)
INSERT INTO track (version, scan_version, track_number, disc_number, name, duration, date, original_date, file_path, file_last_write, file_added, has_cover, mbid, recording_mbid, copyright, copyright_url)
VALUES
(1, 1, 1, 1, 'Orphan Track 1', 180, '2024-05-08', '2024-05-08', '/path/to/orphan_track_1.mp3', '2024-05-08', '2024-05-08', 1, 'mbid_orphan_track_1', 'mbid_recording_orphan_track_1', 'Copyright 2024', 'http://example.com/copyright'),
(2, 1, 2, 1, 'Orphan Track 2', 210, '2024-05-08', '2024-05-08', '/path/to/orphan_track_2.mp3', '2024-05-08', '2024-05-08', 1, 'mbid_orphan_track_2', 'mbid_recording_orphan_track_2', 'Copyright 2024', 'http://example.com/copyright');

-- Inserting tracks with artists but no releases
INSERT INTO track (version, scan_version, track_number, disc_number, name, duration, date, original_date, file_path, file_last_write, file_added, has_cover, mbid, recording_mbid, copyright, copyright_url)
VALUES
(1, 1, 3, 1, 'Track with Artist A', 200, '2024-05-08', '2024-05-08', '/path/to/track_with_artist_a.mp3', '2024-05-08', '2024-05-08', 1, 'mbid_track_with_artist_a', 'mbid_recording_track_with_artist_a', 'Copyright 2024', 'http://example.com/copyright'),
(2, 1, 4, 1, 'Track with Artist B', 220, '2024-05-08', '2024-05-08', '/path/to/track_with_artist_b.mp3', '2024-05-08', '2024-05-08', 1, 'mbid_track_with_artist_b', 'mbid_recording_track_with_artist_b', 'Copyright 2024', 'http://example.com/copyright');

-- Inserting tracks with artists linked
INSERT INTO track_artist_link (version, type, name, track_id, artist_id)
VALUES
(1, 1, 'Artist A', 3, 1),
(2, 1, 'Artist B', 4, 2);

-- Inserting tracks with artists and releases
INSERT INTO track (version, scan_version, track_number, disc_number, name, duration, date, original_date, file_path, file_last_write, file_added, has_cover, mbid, recording_mbid, copyright, copyright_url, release_id, total_disc, total_track, track_replay_gain, release_replay_gain)
VALUES
(1, 1, 5, 1, 'Track with Artist and Release X', 180, '2024-05-08', '2024-05-08', '/path/to/track_with_artist_and_release_x.mp3', '2024-05-08', '2024-05-08', 1, 'mbid_track_with_artist_and_release_x', 'mbid_recording_track_with_artist_and_release_x', 'Copyright 2024', 'http://example.com/copyright', 1, 1, 10, 0, 0),
(1, 1, 6, 1, 'Track with Artist and Release Y', 210, '2024-05-08', '2024-05-08', '/path/to/track_with_artist_and_release_y.mp3', '2024-05-08', '2024-05-08', 1, 'mbid_track_with_artist_and_release_y', 'mbid_recording_track_with_artist_and_release_y', 'Copyright 2024', 'http://example.com/copyright', 1, 1, 10, 0, 0);

-- Inserting tracks with artists and releases linked
INSERT INTO track_artist_link (version, type, name, track_id, artist_id)
VALUES
(1, 1, 'Artist A', 5, 1),
(2, 1, 'Artist B', 6, 2);)" };

        Session session{ db };

        {
            auto transaction{ session.createWriteTransaction() };

            executeStatements(session, schemaV33);
            session.execute("INSERT INTO version_info (version, db_version) VALUES (1, 33)");

            // Insert some base data
            executeStatements(session, createDummyData);
        }

        // Now perform full migration
        db.getTLSSession().migrateSchemaIfNeeded();

        // Now perform some dummy finds to ensure all fields are correctly mapped
        {
            auto transaction{ session.createReadTransaction() };

            EXPECT_FALSE(Artist::find(session, ArtistId{}));
            EXPECT_FALSE(Cluster::find(session, ClusterId{}));
            EXPECT_FALSE(ClusterType::find(session, ClusterTypeId{}));
            EXPECT_FALSE(Directory::find(session, DirectoryId{}));
            EXPECT_FALSE(Image::find(session, ImageId{}));
            EXPECT_FALSE(Label::find(session, LabelId{}));
            EXPECT_FALSE(Listen::find(session, ListenId{}));
            EXPECT_FALSE(RatedArtist::find(session, RatedArtistId{}));
            EXPECT_FALSE(RatedRelease::find(session, RatedReleaseId{}));
            EXPECT_FALSE(RatedTrack::find(session, RatedTrackId{}));
            EXPECT_FALSE(Release::find(session, ReleaseId{}));
            EXPECT_FALSE(ReleaseType::find(session, ReleaseTypeId{}));
            EXPECT_FALSE(StarredArtist::find(session, StarredArtistId{}));
            EXPECT_FALSE(StarredRelease::find(session, StarredReleaseId{}));
            EXPECT_FALSE(StarredTrack::find(session, StarredTrackId{}));
            EXPECT_FALSE(Track::find(session, TrackId{}));
            EXPECT_FALSE(TrackList::find(session, TrackListId{}));
            EXPECT_FALSE(TrackLyrics::find(session, TrackLyricsId{}));
            EXPECT_FALSE(UIState::find(session, UIStateId{}));
            EXPECT_FALSE(User::find(session, UserId{}));
        }
    }
} // namespace lms::db::tests