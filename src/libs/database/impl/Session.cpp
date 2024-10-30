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

#include "core/Exception.hpp"
#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"
#include "database/Artist.hpp"
#include "database/AuthToken.hpp"
#include "database/Cluster.hpp"
#include "database/Db.hpp"
#include "database/Directory.hpp"
#include "database/Image.hpp"
#include "database/Listen.hpp"
#include "database/MediaLibrary.hpp"
#include "database/RatedArtist.hpp"
#include "database/RatedRelease.hpp"
#include "database/RatedTrack.hpp"
#include "database/Release.hpp"
#include "database/ScanSettings.hpp"
#include "database/StarredArtist.hpp"
#include "database/StarredRelease.hpp"
#include "database/StarredTrack.hpp"
#include "database/Track.hpp"
#include "database/TrackArtistLink.hpp"
#include "database/TrackBookmark.hpp"
#include "database/TrackFeatures.hpp"
#include "database/TrackList.hpp"
#include "database/TrackLyrics.hpp"
#include "database/TransactionChecker.hpp"
#include "database/UIState.hpp"
#include "database/User.hpp"

#include "EnumSetTraits.hpp"
#include "Migration.hpp"
#include "PathTraits.hpp"
#include "Utils.hpp"

namespace lms::db
{
    WriteTransaction::WriteTransaction(core::RecursiveSharedMutex& mutex, Wt::Dbo::Session& session)
        : _lock{ mutex }
        , _transaction{ session }
    {
#if LMS_CHECK_TRANSACTION_ACCESSES
        TransactionChecker::pushWriteTransaction(_transaction.session());
#endif
    }

    WriteTransaction::~WriteTransaction()
    {
#if LMS_CHECK_TRANSACTION_ACCESSES
        TransactionChecker::popWriteTransaction(_transaction.session());
#endif

        core::tracing::ScopedTrace _trace{ "Database", core::tracing::Level::Detailed, "Commit" };
        _transaction.commit();
    }

    ReadTransaction::ReadTransaction(Wt::Dbo::Session& session)
        : _transaction{ session }
    {
#if LMS_CHECK_TRANSACTION_ACCESSES
        TransactionChecker::pushReadTransaction(_transaction.session());
#endif
    }

    ReadTransaction::~ReadTransaction()
    {
#if LMS_CHECK_TRANSACTION_ACCESSES
        TransactionChecker::popReadTransaction(_transaction.session());
#endif
    }

    Session::Session(Db& db)
        : _db{ db }
    {
        _session.setConnectionPool(_db.getConnectionPool());

        _session.mapClass<VersionInfo>("version_info");
        _session.mapClass<Artist>("artist");
        _session.mapClass<AuthToken>("auth_token");
        _session.mapClass<Cluster>("cluster");
        _session.mapClass<ClusterType>("cluster_type");
        _session.mapClass<Directory>("directory");
        _session.mapClass<Image>("image");
        _session.mapClass<Label>("label");
        _session.mapClass<Listen>("listen");
        _session.mapClass<MediaLibrary>("media_library");
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
        _session.mapClass<TrackFeatures>("track_features");
        _session.mapClass<TrackList>("tracklist");
        _session.mapClass<TrackListEntry>("tracklist_entry");
        _session.mapClass<TrackLyrics>("track_lyrics");
        _session.mapClass<UIState>("ui_state");
        _session.mapClass<User>("user");
    }

    WriteTransaction Session::createWriteTransaction()
    {
        return WriteTransaction{ _db.getMutex(), _session };
    }

    ReadTransaction Session::createReadTransaction()
    {
        return ReadTransaction{ _session };
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
            ScanSettings::init(*this);
        }

        return migrationPerformed;
    }

    void Session::createIndexesIfNeeded()
    {
        LMS_SCOPED_TRACE_OVERVIEW("Database", "IndexCreation");
        LMS_LOG(DB, INFO, "Creating indexes... This may take a while...");

        auto transaction{ createWriteTransaction() };
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS artist_id_idx ON artist(id)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS artist_image_idx ON artist(image_id)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS artist_name_idx ON artist(name)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS artist_sort_name_nocase_idx ON artist(sort_name COLLATE NOCASE)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS artist_mbid_idx ON artist(mbid)");

        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS auth_token_user_idx ON auth_token(user_id)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS auth_token_expiry_idx ON auth_token(expiry)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS auth_token_value_idx ON auth_token(value)");

        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS cluster_cluster_type_idx ON cluster(cluster_type_id)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS cluster_type_name_idx ON cluster_type(name)");

        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS directory_id_idx ON directory(id)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS directory_parent_directory_idx ON directory(parent_directory_id)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS directory_path_idx ON directory(absolute_path)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS directory_media_library_idx ON directory(media_library_id)");

        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS image_directory_stem_idx ON image(directory_id, stem COLLATE NOCASE)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS image_id_idx ON image(id)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS image_path_idx ON image(absolute_file_path)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS image_stem_idx ON image(stem COLLATE NOCASE)");

        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS label_name_idx ON label(name)");

        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS listen_backend_idx ON listen(backend)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS listen_id_idx ON listen(id)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS listen_user_backend_idx ON listen(user_id,backend)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS listen_user_backend_date_time_idx ON listen(user_id, backend, date_time DESC)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS listen_track_user_backend_idx ON listen(track_id,user_id,backend)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS listen_user_track_backend_date_time_idx ON listen(user_id,track_id,backend,date_time)");

        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS media_library_id_idx ON media_library(id)");

        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS rated_artist_user_artist_idx ON rated_artist(user_id,artist_id)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS rated_release_user_release_idx ON rated_release(user_id,release_id)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS rated_track_user_track_idx ON rated_track(user_id,track_id)");

        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS release_id_idx ON release(id)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS release_image_idx ON release(image_id)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS release_mbid_idx ON release(mbid)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS release_name_idx ON release(name)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS release_name_nocase_idx ON release(name COLLATE NOCASE)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS release_type_name_idx ON release_type(name)");

        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_id_idx ON track(id)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_absolute_path_idx ON track(absolute_file_path)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_date_idx ON track(date)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_directory_release_idx ON track(directory_id, release_id);");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_directory_file_stem_idx ON track(directory_id, file_stem);");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_file_last_write_idx ON track(file_last_write)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_media_library_idx ON track(media_library_id)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_media_library_release_idx ON track(media_library_id, release_id)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_mbid_idx ON track(mbid)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_name_idx ON track(name)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_name_nocase_idx ON track(name COLLATE NOCASE)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_original_date_idx ON track(original_date)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_original_year_idx ON track(original_year)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_recording_mbid_idx ON track(recording_mbid)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_release_idx ON track(release_id)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_release_file_last_write_idx ON track(release_id, file_last_write)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_release_year_idx ON track(release_id, year)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_year_idx ON track(year)");

        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS tracklist_name_idx ON tracklist(name)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS tracklist_user_idx ON tracklist(user_id)");

        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_artist_link_artist_idx ON track_artist_link(artist_id)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_artist_link_artist_track_idx ON track_artist_link(artist_id, track_id)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_artist_link_artist_type_idx ON track_artist_link(artist_id,type)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_artist_link_track_artist_idx ON track_artist_link(track_id, artist_id)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_artist_link_track_type_idx ON track_artist_link(track_id,type)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_artist_link_type_track_artist_idx ON track_artist_link(type, track_id, artist_id)");

        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_features_track_idx ON track_features(track_id)");

        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_lyrics_id_idx ON track_lyrics(id)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_lyrics_absolute_file_path_idx ON track_lyrics(absolute_file_path)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_lyrics_track_idx ON track_lyrics(track_id)");

        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_bookmark_user_idx ON track_bookmark(user_id)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS track_bookmark_user_track_idx ON track_bookmark(user_id,track_id)");

        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS starred_artist_user_backend_idx ON starred_artist(user_id,backend)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS starred_artist_artist_user_backend_idx ON starred_artist(artist_id,user_id,backend)");

        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS starred_release_user_backend_idx ON starred_release(user_id,backend)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS starred_release_release_user_backend_idx ON starred_release(release_id,user_id,backend)");

        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS starred_track_user_backend_idx ON starred_track(user_id,backend)");
        utils::executeCommand(_session, "CREATE INDEX IF NOT EXISTS starred_track_track_user_backend_idx ON starred_track(track_id,user_id,backend)");

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
            std::unique_lock lock{ _db.getMutex() };
            _db.executeSql("VACUUM");
        }

        LMS_LOG(DB, INFO, "Vacuum complete!");
    }

    void Session::refreshTracingLoggerStats()
    {
        auto* traceLogger{ core::Service<core::tracing::ITraceLogger>::get() };
        if (!traceLogger)
            return;

        auto transaction{ createReadTransaction() };

        traceLogger->setMetadata("db_artist_count", std::to_string(db::Artist::getCount(*this)));
        traceLogger->setMetadata("db_cluster_count", std::to_string(db::Cluster::getCount(*this)));
        traceLogger->setMetadata("db_cluster_type_count", std::to_string(db::ClusterType::getCount(*this)));
        traceLogger->setMetadata("db_starred_artist_count", std::to_string(db::StarredArtist::getCount(*this)));
        traceLogger->setMetadata("db_starred_release_count", std::to_string(db::StarredRelease::getCount(*this)));
        traceLogger->setMetadata("db_starred_track_count", std::to_string(db::StarredTrack::getCount(*this)));
        traceLogger->setMetadata("db_track_bookmark_count", std::to_string(db::TrackBookmark::getCount(*this)));
        traceLogger->setMetadata("db_listen_count", std::to_string(db::Listen::getCount(*this)));
        traceLogger->setMetadata("db_release_count", std::to_string(db::Release::getCount(*this)));
        traceLogger->setMetadata("db_track_count", std::to_string(db::Track::getCount(*this)));
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
} // namespace lms::db
