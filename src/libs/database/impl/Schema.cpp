/*
 * Copyright (C) 2019 Emeric Poupon
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

#include <string>
#include <string_view>

#include <Wt/Dbo/Exception.h>

#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"

#include "Schema.hpp"
#include "Session.hpp"
#include "Utils.hpp"

namespace lms::db::schema
{
    namespace
    {
        constexpr std::string_view indexSqls[]{
            "CREATE INDEX IF NOT EXISTS artist_name_mbid_idx ON artist(name, mbid)",
            "CREATE INDEX IF NOT EXISTS artist_name_nocase_idx ON artist(name COLLATE NOCASE)",
            "CREATE INDEX IF NOT EXISTS artist_sort_name_nocase_idx ON artist(COALESCE(NULLIF(sort_name, ''), name) COLLATE NOCASE)",
            "CREATE INDEX IF NOT EXISTS artist_mbid_idx ON artist(mbid)",
            "CREATE INDEX IF NOT EXISTS artist_preferred_artwork_idx ON artist(preferred_artwork_id) WHERE preferred_artwork_id IS NOT NULL",

            "CREATE INDEX IF NOT EXISTS artist_feedback_user_idx ON artist_feedback(user_id)",
            "CREATE UNIQUE INDEX IF NOT EXISTS artist_feedback_artist_user_idx ON artist_feedback(artist_id,user_id)",

            "CREATE UNIQUE INDEX IF NOT EXISTS artist_feedback_backend_sync_artist_feedback_backend_idx ON artist_feedback_backend_sync(artist_feedback_id,backend)",
            "CREATE INDEX IF NOT EXISTS artist_feedback_backend_sync_backend_sync_state_idx ON artist_feedback_backend_sync(backend,sync_state)",

            "CREATE INDEX IF NOT EXISTS artist_info_path_idx ON artist_info(absolute_file_path)",
            "CREATE INDEX IF NOT EXISTS artist_info_directory_id_idx ON artist_info(directory_id)",
            "CREATE INDEX IF NOT EXISTS artist_info_artist_id_idx ON artist_info(artist_id)",
            "CREATE INDEX IF NOT EXISTS artist_info_mbid_matched_artist_idx ON artist_info(mbid_matched, artist_id)",

            "CREATE INDEX IF NOT EXISTS artwork_image_idx ON artwork(image_id)",
            "CREATE INDEX IF NOT EXISTS artwork_track_embedded_image_idx ON artwork(track_embedded_image_id)",

            "CREATE INDEX IF NOT EXISTS auth_token_user_domain_idx ON auth_token(user_id, domain)",
            "CREATE INDEX IF NOT EXISTS auth_token_domain_expiry_idx ON auth_token(domain, expiry)",
            "CREATE INDEX IF NOT EXISTS auth_token_domain_value_idx ON auth_token(domain, value)",

            "CREATE INDEX IF NOT EXISTS cluster_cluster_type_name_idx ON cluster(cluster_type_id, name)",
            "CREATE INDEX IF NOT EXISTS cluster_type_name_idx ON cluster_type(name)",

            "CREATE INDEX IF NOT EXISTS country_name_idx ON country(name)",

            "CREATE INDEX IF NOT EXISTS directory_parent_directory_idx ON directory(parent_directory_id)",
            "CREATE INDEX IF NOT EXISTS directory_path_idx ON directory(absolute_path)",
            "CREATE INDEX IF NOT EXISTS directory_media_library_idx ON directory(media_library_id)",
            "CREATE INDEX IF NOT EXISTS directory_name_idx ON directory(name COLLATE NOCASE)",

            "CREATE UNIQUE INDEX IF NOT EXISTS genre_name_idx ON genre(name)",

            "CREATE UNIQUE INDEX IF NOT EXISTS grouping_name_idx ON grouping(name)",

            "CREATE INDEX IF NOT EXISTS image_directory_stem_idx ON image(directory_id, stem COLLATE NOCASE)",
            "CREATE INDEX IF NOT EXISTS image_path_idx ON image(absolute_file_path)",
            "CREATE INDEX IF NOT EXISTS image_stem_idx ON image(stem COLLATE NOCASE)",

            "CREATE INDEX IF NOT EXISTS label_name_idx ON label(name COLLATE NOCASE)",

            "CREATE UNIQUE INDEX IF NOT EXISTS language_name_idx ON language(name)",

            "CREATE INDEX IF NOT EXISTS listen_user_track_date_time_idx ON listen(user_id, track_id, date_time)",
            "CREATE INDEX IF NOT EXISTS listen_track_idx ON listen(track_id)",

            "CREATE UNIQUE INDEX IF NOT EXISTS listen_backend_sync_listen_backend_idx ON listen_backend_sync(listen_id, backend)",
            "CREATE INDEX IF NOT EXISTS listen_backend_sync_backend_sync_state_idx ON listen_backend_sync(backend, sync_state)",

            "CREATE INDEX IF NOT EXISTS medium_release_position_idx ON medium(release_id, position)",
            "CREATE INDEX IF NOT EXISTS medium_preferred_artwork_idx ON medium(preferred_artwork_id) WHERE preferred_artwork_id IS NOT NULL",

            "CREATE UNIQUE INDEX IF NOT EXISTS mood_name_idx ON mood(name)",

            "CREATE INDEX IF NOT EXISTS playlist_file_directory_idx ON playlist_file(directory_id)",
            "CREATE INDEX IF NOT EXISTS playlist_file_absolute_file_path_idx ON playlist_file(absolute_file_path)",
            "CREATE INDEX IF NOT EXISTS playlist_file_media_library_idx ON playlist_file(media_library_id)",
            "CREATE INDEX IF NOT EXISTS playlist_file_preferred_artwork_idx ON playlist_file(preferred_artwork_id) WHERE preferred_artwork_id IS NOT NULL",

            "CREATE INDEX IF NOT EXISTS playqueue_user_type_idx ON playqueue(user_id, type)",
            "CREATE INDEX IF NOT EXISTS playqueue_tracklist_idx ON playqueue(tracklist_id)",

            "CREATE INDEX IF NOT EXISTS podcast_url_idx ON podcast(url)",
            "CREATE INDEX IF NOT EXISTS podcast_artwork_idx ON podcast(artwork_id) WHERE artwork_id IS NOT NULL",

            "CREATE INDEX IF NOT EXISTS podcast_episode_podcast_pub_date_idx ON podcast_episode(podcast_id, pub_date)",
            "CREATE INDEX IF NOT EXISTS podcast_episode_artwork_idx ON podcast_episode(artwork_id) WHERE artwork_id IS NOT NULL",

            "CREATE INDEX IF NOT EXISTS rated_artist_user_artist_idx ON rated_artist(user_id,artist_id)",
            "CREATE INDEX IF NOT EXISTS rated_artist_artist_idx ON rated_artist(artist_id)",

            "CREATE INDEX IF NOT EXISTS rated_release_user_release_idx ON rated_release(user_id,release_id)",
            "CREATE INDEX IF NOT EXISTS rated_release_release_idx ON rated_release(release_id)",

            "CREATE INDEX IF NOT EXISTS rated_track_user_track_idx ON rated_track(user_id,track_id)",
            "CREATE INDEX IF NOT EXISTS rated_track_track_idx ON rated_track(track_id)",

            "CREATE INDEX IF NOT EXISTS release_group_mbid_idx ON release(group_mbid)",
            "CREATE INDEX IF NOT EXISTS release_mbid_idx ON release(mbid)",
            "CREATE INDEX IF NOT EXISTS release_name_idx ON release(name)",
            "CREATE INDEX IF NOT EXISTS release_name_nocase_idx ON release(name COLLATE NOCASE)",
            "CREATE INDEX IF NOT EXISTS release_sort_name_nocase_idx ON release(COALESCE(NULLIF(sort_name, ''), name) COLLATE NOCASE)",
            "CREATE INDEX IF NOT EXISTS release_preferred_artwork_idx ON release(preferred_artwork_id) WHERE preferred_artwork_id IS NOT NULL",

            "CREATE INDEX IF NOT EXISTS release_artist_link_artist_idx ON release_artist_link(artist_id)",
            "CREATE INDEX IF NOT EXISTS release_artist_link_release_idx ON release_artist_link(release_id)",
            "CREATE INDEX IF NOT EXISTS release_artist_link_artist_mbid_matched_artist_idx ON release_artist_link(artist_mbid_matched, artist_id)",

            "CREATE INDEX IF NOT EXISTS release_feedback_user_idx ON release_feedback(user_id)",
            "CREATE UNIQUE INDEX IF NOT EXISTS release_feedback_release_user_idx ON release_feedback(release_id,user_id)",

            "CREATE UNIQUE INDEX IF NOT EXISTS release_feedback_backend_sync_release_feedback_backend_idx ON release_feedback_backend_sync(release_feedback_id,backend)",
            "CREATE INDEX IF NOT EXISTS release_feedback_backend_sync_backend_sync_state_idx ON release_feedback_backend_sync(backend,sync_state)",

            "CREATE INDEX IF NOT EXISTS release_type_name_idx ON release_type(name COLLATE NOCASE)",

            "CREATE INDEX IF NOT EXISTS track_absolute_path_idx ON track(absolute_file_path)",
            "CREATE INDEX IF NOT EXISTS track_date_idx ON track(date)",
            "CREATE INDEX IF NOT EXISTS track_directory_release_idx ON track(directory_id, release_id)",
            "CREATE INDEX IF NOT EXISTS track_file_added_idx ON track(file_added)",
            "CREATE INDEX IF NOT EXISTS track_file_last_write_idx ON track(file_last_write)",
            "CREATE INDEX IF NOT EXISTS track_media_library_release_idx ON track(media_library_id, release_id)",
            "CREATE INDEX IF NOT EXISTS track_medium_idx ON track(medium_id)",
            "CREATE INDEX IF NOT EXISTS track_mbid_idx ON track(mbid)",
            "CREATE INDEX IF NOT EXISTS track_name_file_size_idx ON track(name, file_size)",
            "CREATE INDEX IF NOT EXISTS track_name_nocase_idx ON track(name COLLATE NOCASE)",
            "CREATE INDEX IF NOT EXISTS track_recording_mbid_idx ON track(recording_mbid)",
            "CREATE INDEX IF NOT EXISTS track_release_date_idx ON track(release_id, date)",
            "CREATE INDEX IF NOT EXISTS track_release_original_date_idx ON track(release_id, original_date)",
            "CREATE INDEX IF NOT EXISTS track_release_file_last_write_idx ON track(release_id, file_last_write)",
            "CREATE INDEX IF NOT EXISTS track_release_file_added_idx ON track(release_id, file_added)",
            "CREATE INDEX IF NOT EXISTS track_preferred_artwork_idx ON track(preferred_artwork_id) WHERE preferred_artwork_id IS NOT NULL",
            "CREATE INDEX IF NOT EXISTS track_preferred_media_artwork_idx ON track(preferred_media_artwork_id) WHERE preferred_media_artwork_id IS NOT NULL",

            "CREATE INDEX IF NOT EXISTS track_artist_link_artist_mbid_matched_artist_idx ON track_artist_link(artist_mbid_matched, artist_id)",
            "CREATE INDEX IF NOT EXISTS track_artist_link_artist_type_track_idx ON track_artist_link(artist_id, type, track_id)",
            "CREATE INDEX IF NOT EXISTS track_artist_link_track_type_artist_idx ON track_artist_link(track_id, type, artist_id)",

            "CREATE INDEX IF NOT EXISTS track_bookmark_user_track_idx ON track_bookmark(user_id,track_id)",
            "CREATE INDEX IF NOT EXISTS track_bookmark_track_idx ON track_bookmark(track_id)",

            "CREATE INDEX IF NOT EXISTS track_embedded_image_hash_idx ON track_embedded_image(hash)",

            "CREATE INDEX IF NOT EXISTS track_embedded_image_link_track_embedded_image_id_track_id_idx ON track_embedded_image_link(track_embedded_image_id, track_id)",
            "CREATE INDEX IF NOT EXISTS track_embedded_image_link_track_track_embedded_image_id_idx ON track_embedded_image_link(track_id, track_embedded_image_id)",

            "CREATE INDEX IF NOT EXISTS track_feedback_user_idx ON track_feedback(user_id)",
            "CREATE UNIQUE INDEX IF NOT EXISTS track_feedback_track_user_idx ON track_feedback(track_id,user_id)",

            "CREATE UNIQUE INDEX IF NOT EXISTS track_feedback_backend_sync_track_feedback_backend_idx ON track_feedback_backend_sync(track_feedback_id,backend)",
            "CREATE INDEX IF NOT EXISTS track_feedback_backend_sync_backend_sync_state_idx ON track_feedback_backend_sync(backend,sync_state)",

            "CREATE INDEX IF NOT EXISTS track_lyrics_absolute_file_path_idx ON track_lyrics(absolute_file_path)",
            "CREATE INDEX IF NOT EXISTS track_lyrics_directory_idx ON track_lyrics(directory_id)",
            "CREATE INDEX IF NOT EXISTS track_lyrics_track_idx ON track_lyrics(track_id)",

            "CREATE INDEX IF NOT EXISTS track_movement_track_idx ON track_movement(track_id)",

            "CREATE INDEX IF NOT EXISTS track_musicnn_embeddings_track_idx ON track_musicnn_embeddings(track_id)",

            "CREATE INDEX IF NOT EXISTS tracklist_user_type_idx ON tracklist(user_id, type)",
            "CREATE INDEX IF NOT EXISTS tracklist_last_modified_date_time_idx ON tracklist(last_modified_date_time)",
            "CREATE INDEX IF NOT EXISTS tracklist_playlist_file_idx ON tracklist(playlist_file_id)",

            "CREATE INDEX IF NOT EXISTS tracklist_entry_tracklist_track_idx ON tracklist_entry(tracklist_id, track_id)",
            "CREATE INDEX IF NOT EXISTS tracklist_entry_track_idx ON tracklist_entry(track_id)",

            "CREATE INDEX IF NOT EXISTS ui_state_user_item_idx ON ui_state(user_id, item)",

            "CREATE INDEX IF NOT EXISTS work_mbid_idx ON work(mbid)",
        };
    } // namespace

    void createTablesIfNeeded(ISession& session)
    {
        LMS_LOG(DB, INFO, "Preparing tables...");

        // Initial creation case
        try
        {
            auto transaction{ session.createWriteTransaction() };
            getDboSession(session)->createTables();
            LMS_LOG(DB, INFO, "Tables created");
        }
        catch (Wt::Dbo::Exception& e)
        {
            LMS_LOG(DB, DEBUG, "Cannot create tables: " << e.what());
            if (std::string_view{ e.what() }.find("already exists") == std::string_view::npos)
            {
                LMS_LOG(DB, ERROR, "Cannot create tables: " << e.what());
                throw e;
            }
        }
    }

    void createIndexesIfNeeded(ISession& session)
    {
        LMS_SCOPED_TRACE_OVERVIEW("Database", "IndexCreation");
        LMS_LOG(DB, INFO, "Creating indexes... This may take a while...");

        for (std::string_view sql : indexSqls)
        {
            auto transaction{ session.createWriteTransaction() };
            utils::executeCommand(*getDboSession(session), std::string{ sql });
        }

        LMS_LOG(DB, INFO, "Indexes created!");
    }
} // namespace lms::db::schema
