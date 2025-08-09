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

#include "database/Session.hpp"

#include <Wt/Dbo/WtSqlTraits.h>

#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"

#include "database/objects/Artist.hpp"
#include "database/objects/ArtistInfo.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/AuthToken.hpp"
#include "database/objects/Cluster.hpp"
#include "database/objects/Directory.hpp"
#include "database/objects/Image.hpp"
#include "database/objects/Listen.hpp"
#include "database/objects/MediaLibrary.hpp"
#include "database/objects/Medium.hpp"
#include "database/objects/PlayListFile.hpp"
#include "database/objects/PlayQueue.hpp"
#include "database/objects/RatedArtist.hpp"
#include "database/objects/RatedRelease.hpp"
#include "database/objects/RatedTrack.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/ScanSettings.hpp"
#include "database/objects/StarredArtist.hpp"
#include "database/objects/StarredRelease.hpp"
#include "database/objects/StarredTrack.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackArtistLink.hpp"
#include "database/objects/TrackBookmark.hpp"
#include "database/objects/TrackEmbeddedImage.hpp"
#include "database/objects/TrackEmbeddedImageLink.hpp"
#include "database/objects/TrackFeatures.hpp"
#include "database/objects/TrackList.hpp"
#include "database/objects/TrackLyrics.hpp"
#include "database/objects/UIState.hpp"
#include "database/objects/User.hpp"

#include "Db.hpp"
#include "Migration.hpp"
#include "TransactionChecker.hpp"
#include "Utils.hpp"
#include "traits/EnumSetTraits.hpp"
#include "traits/ImageHashTypeTraits.hpp"
#include "traits/PartialDateTimeTraits.hpp"
#include "traits/PathTraits.hpp"

namespace lms::db
{
    Session::Session(IDb& db)
        : _db{ db }
    {
        _session.setConnectionPool(static_cast<Db&>(_db).getConnectionPool());

        _session.mapClass<Artist>("artist");
        _session.mapClass<ArtistInfo>("artist_info");
        _session.mapClass<Artwork>("artwork");
        _session.mapClass<AuthToken>("auth_token");
        _session.mapClass<Cluster>("cluster");
        _session.mapClass<ClusterType>("cluster_type");
        _session.mapClass<Country>("country");
        _session.mapClass<Directory>("directory");
        _session.mapClass<Image>("image");
        _session.mapClass<Label>("label");
        _session.mapClass<Listen>("listen");
        _session.mapClass<MediaLibrary>("media_library");
        _session.mapClass<Medium>("medium");
        _session.mapClass<PlayListFile>("playlist_file");
        _session.mapClass<PlayQueue>("playqueue");
        _session.mapClass<RatedArtist>("rated_artist");
        _session.mapClass<RatedRelease>("rated_release");
        _session.mapClass<RatedTrack>("rated_track");
        _session.mapClass<Release>("release");
        _session.mapClass<ReleaseType>("release_type");
        _session.mapClass<ScanSettings>("scan_settings");
        _session.mapClass<StarredArtist>("starred_artist");
        _session.mapClass<StarredRelease>("starred_release");
        _session.mapClass<StarredTrack>("starred_track");
        _session.mapClass<Track>("track");
        _session.mapClass<TrackBookmark>("track_bookmark");
        _session.mapClass<TrackArtistLink>("track_artist_link");
        _session.mapClass<TrackEmbeddedImage>("track_embedded_image");
        _session.mapClass<TrackEmbeddedImageLink>("track_embedded_image_link");
        _session.mapClass<TrackFeatures>("track_features");
        _session.mapClass<TrackList>("tracklist");
        _session.mapClass<TrackListEntry>("tracklist_entry");
        _session.mapClass<TrackLyrics>("track_lyrics");
        _session.mapClass<UIState>("ui_state");
        _session.mapClass<User>("user");
        _session.mapClass<VersionInfo>("version_info");
    }

    WriteTransaction Session::createWriteTransaction()
    {
        return WriteTransaction{ static_cast<Db&>(_db).getMutex(), _session };
    }

    ReadTransaction Session::createReadTransaction()
    {
        return ReadTransaction{ _session };
    }

    void Session::checkWriteTransaction() const
    {
#if LMS_CHECK_TRANSACTION_ACCESSES
        TransactionChecker::checkWriteTransaction(_session);
#endif
    }
    void Session::checkReadTransaction() const
    {
#if LMS_CHECK_TRANSACTION_ACCESSES
        TransactionChecker::checkReadTransaction(_session);
#endif
    }

    void Session::execute(std::string_view statement)
    {
        utils::executeCommand(_session, std::string{ statement });
    }

    void Session::prepareTablesIfNeeded()
    {
        LMS_LOG(DB, INFO, "Preparing tables...");

        // Initial creation case
        try
        {
            auto transaction{ createWriteTransaction() };
            _session.createTables();
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

    bool Session::migrateSchemaIfNeeded()
    {
        const bool migrationPerformed{ Migration::doDbMigration(*this) };

        // TODO: move this elsewhere
        {
            auto uniqueTransaction{ createWriteTransaction() };

            if (!ScanSettings::find(*this))
                create<ScanSettings>();
        }

        return migrationPerformed;
    }

    void Session::createIndexesIfNeeded()
    {
        LMS_SCOPED_TRACE_OVERVIEW("Database", "IndexCreation");
        LMS_LOG(DB, INFO, "Creating indexes... This may take a while...");

        {
            auto transaction{ createWriteTransaction() };
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS artist_id_idx ON artist(id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS artist_name_mbid_idx ON artist(name, mbid)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS artist_sort_name_nocase_idx ON artist(sort_name COLLATE NOCASE)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS artist_mbid_idx ON artist(mbid)");

            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS artist_info_path_idx ON artist_info(absolute_file_path)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS artist_info_directory_id_idx ON artist_info(directory_id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS artist_info_artist_id_idx ON artist_info(artist_id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS artist_info_mbid_matched_artist_idx ON artist_info(mbid_matched, artist_id)");

            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS artwork_id_idx ON artwork(id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS artwork_image_idx ON artwork(image_id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS artwork_track_embedded_image_idx ON artwork(track_embedded_image_id)");

            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS auth_token_user_domain_idx ON auth_token(user_id, domain)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS auth_token_domain_expiry_idx ON auth_token(domain, expiry)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS auth_token_domain_value_idx ON auth_token(domain, value)");

            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS cluster_id_idx ON cluster(id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS cluster_cluster_type_idx ON cluster(cluster_type_id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS cluster_type_name_idx ON cluster_type(name)");

            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS country_id_idx ON country(id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS country_name_idx ON country(name COLLATE NOCASE)");

            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS directory_id_idx ON directory(id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS directory_parent_directory_idx ON directory(parent_directory_id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS directory_path_idx ON directory(absolute_path)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS directory_media_library_idx ON directory(media_library_id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS directory_name_idx ON directory(name COLLATE NOCASE)");

            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_embedded_image_id_idx ON track_embedded_image(id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_embedded_image_hash_idx ON track_embedded_image(hash)");

            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_embedded_image_link_id_idx ON track_embedded_image_link(id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_embedded_image_link_track_id_idx ON track_embedded_image_link(track_id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_embedded_image_link_track_embedded_image_id_track_id_idx ON track_embedded_image_link(track_embedded_image_id, track_id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_embedded_image_link_track_track_embedded_image_id_idx ON track_embedded_image_link(track_id, track_embedded_image_id)");

            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS image_directory_stem_idx ON image(directory_id, stem COLLATE NOCASE)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS image_id_idx ON image(id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS image_path_idx ON image(absolute_file_path)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS image_stem_idx ON image(stem COLLATE NOCASE)");

            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS label_id_idx ON label(id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS label_name_idx ON label(name COLLATE NOCASE)");

            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS listen_backend_idx ON listen(backend)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS listen_id_idx ON listen(id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS listen_user_backend_idx ON listen(user_id,backend)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS listen_user_backend_date_time_idx ON listen(user_id, backend, date_time DESC)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS listen_track_user_backend_idx ON listen(track_id,user_id,backend)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS listen_user_track_backend_date_time_idx ON listen(user_id,track_id,backend,date_time)");

            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS media_library_id_idx ON media_library(id)");

            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS medium_release_position_idx ON medium(release_id, position)");

            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS playlist_file_id_idx ON playlist_file(id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS playlist_file_directory_idx ON playlist_file(directory_id);");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS playlist_file_absolute_file_path_idx ON playlist_file(absolute_file_path)");

            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS rated_artist_user_artist_idx ON rated_artist(user_id,artist_id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS rated_release_user_release_idx ON rated_release(user_id,release_id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS rated_track_user_track_idx ON rated_track(user_id,track_id)");

            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS release_id_idx ON release(id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS release_group_mbid_idx ON release(group_mbid)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS release_mbid_idx ON release(mbid)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS release_name_idx ON release(name)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS release_name_nocase_idx ON release(name COLLATE NOCASE)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS release_sort_name_idx ON release(sort_name)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS release_sort_name_nocase_idx ON release(sort_name COLLATE NOCASE)");

            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS release_type_id_idx ON release_type(id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS release_type_name_idx ON release_type(name COLLATE NOCASE)");

            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_id_idx ON track(id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_absolute_path_idx ON track(absolute_file_path)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_date_idx ON track(date)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_directory_release_idx ON track(directory_id, release_id);");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_file_added_idx ON track(file_added)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_file_last_write_idx ON track(file_last_write)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_media_library_idx ON track(media_library_id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_media_library_release_idx ON track(media_library_id, release_id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_medium_idx ON track(medium_id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_mbid_idx ON track(mbid)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_name_file_size_idx ON track(name, file_size)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_name_nocase_idx ON track(name COLLATE NOCASE)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_original_date_idx ON track(original_date)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_recording_mbid_idx ON track(recording_mbid)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_release_date_idx ON track(release_id, date)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_release_file_last_write_idx ON track(release_id, file_last_write)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_release_file_added_idx ON track(release_id, file_added)");

            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS tracklist_id_idx ON tracklist(id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS tracklist_name_idx ON tracklist(name)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS tracklist_user_type_idx ON tracklist(user_id, type)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS tracklist_last_modified_date_time_idx ON tracklist(last_modified_date_time)");

            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS tracklist_entry_idx ON tracklist_entry(id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS tracklist_entry_tracklist_track_idx ON tracklist_entry(tracklist_id, track_id)");

            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_artist_link_id_idx ON track_artist_link(id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_artist_link_artist_idx ON track_artist_link(artist_id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_artist_link_artist_mbid_matched_artist_idx ON track_artist_link(artist_mbid_matched, artist_id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_artist_link_artist_track_idx ON track_artist_link(artist_id, track_id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_artist_link_artist_type_track_idx ON track_artist_link(artist_id, type, track_id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_artist_link_track_artist_idx ON track_artist_link(track_id, artist_id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_artist_link_track_type_idx ON track_artist_link(track_id, type)");

            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_features_track_idx ON track_features(track_id)");

            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_lyrics_id_idx ON track_lyrics(id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_lyrics_absolute_file_path_idx ON track_lyrics(absolute_file_path)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_lyrics_directory_idx ON track_lyrics(directory_id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_lyrics_track_idx ON track_lyrics(track_id)");

            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_bookmark_user_idx ON track_bookmark(user_id)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_bookmark_user_track_idx ON track_bookmark(user_id,track_id)");

            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS starred_artist_user_backend_idx ON starred_artist(user_id,backend)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS starred_artist_artist_user_backend_idx ON starred_artist(artist_id,user_id,backend)");

            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS starred_release_user_backend_idx ON starred_release(user_id,backend)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS starred_release_release_user_backend_idx ON starred_release(release_id,user_id,backend)");

            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS starred_track_user_backend_idx ON starred_track(user_id,backend)");
            utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS starred_track_track_user_backend_idx ON starred_track(track_id,user_id,backend)");
        }

        LMS_LOG(DB, INFO, "Indexes created!");
    }

    void Session::vacuumIfNeeded()
    {
        long pageCount{};
        long freeListCount{};

        {
            auto transaction{ createReadTransaction() };
            pageCount = utils::fetchQuerySingleResult(_session.query<long>("SELECT page_count FROM pragma_page_count"));
            freeListCount = utils::fetchQuerySingleResult(_session.query<long>("SELECT freelist_count FROM pragma_freelist_count"));
        }

        LMS_LOG(DB, INFO, "page stats: page_count = " << pageCount << ", freelist_count = " << freeListCount);
        if (freeListCount >= (pageCount / 10))
            vacuum();
    }

    void Session::vacuum()
    {
        LMS_SCOPED_TRACE_OVERVIEW("Database", "Vacuum");
        LMS_LOG(DB, INFO, "Performing vacuum... This may take a while...");

        // We manually take a lock here since vacuum cannot be inside a transaction
        {
            std::unique_lock lock{ static_cast<Db&>(_db).getMutex() };
            static_cast<Db&>(_db).executeSql("VACUUM");
        }

        LMS_LOG(DB, INFO, "Vacuum complete!");
    }

    void Session::fullAnalyze()
    {
        LMS_SCOPED_TRACE_OVERVIEW("Database", "Analyze");
        LMS_LOG(DB, INFO, "Performing database analyze... This may take a while...");

        // first select all the tables and indexes, and then analyze one by one in order to not have a big lock
        std::vector<std::string> entries;
        retrieveEntriesToAnalyze(entries);

        for (const std::string& entry : entries)
            analyzeEntry(entry);

        LMS_LOG(DB, INFO, "Analyze complete!");
    }

    bool Session::areAllTablesEmpty()
    {
        checkReadTransaction();

        const std::vector<std::string> entryList{ utils::fetchQueryResults(_session.query<std::string>("SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'")) };

        return std::all_of(entryList.cbegin(), entryList.cend(), [this](const std::string& entry) {
            const auto count{ utils::fetchQuerySingleResult(_session.query<long>("SELECT COUNT(*) FROM " + entry)) };
            return count == 0;
        });
    }

    FileStats Session::getFileStats()
    {
        FileStats stats{};

        stats.trackCount = db::Track::getCount(*this);
        stats.artistInfoCount = db::ArtistInfo::getCount(*this);
        stats.imageCount = db::Image::getCount(*this);
        stats.trackLyricsCount = db::TrackLyrics::getExternalLyricsCount(*this);
        stats.playListCount = db::PlayListFile::getCount(*this);

        return stats;
    }

    void Session::retrieveEntriesToAnalyze(std::vector<std::string>& entryList)
    {
        auto transaction{ createReadTransaction() };
        entryList = utils::fetchQueryResults(_session.query<std::string>("SELECT name FROM sqlite_master WHERE type='table' OR type ='index'"));
    }

    void Session::analyzeEntry(const std::string& entry)
    {
        LMS_LOG(DB, DEBUG, "Analyzing " << entry);
        {
            auto transaction{ createWriteTransaction() };
            utils::executeCommand(_session, "ANALYZE " + entry);
        }
        LMS_LOG(DB, DEBUG, "Analyzing " << entry << ": done!");
    }

    void Session::execute(std::string_view query, long long id)
    {
        utils::executeCommand(_session, query, id);
    }

} // namespace lms::db
