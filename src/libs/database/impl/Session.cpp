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

#include <cassert>

#include "core/Exception.hpp"
#include "core/ILogger.hpp"
#include "core/ITraceLogger.hpp"

#include "database/Artist.hpp"
#include "database/AuthToken.hpp"
#include "database/Cluster.hpp"
#include "database/Db.hpp"
#include "database/Listen.hpp"
#include "database/MediaLibrary.hpp"
#include "database/Release.hpp"
#include "database/ScanSettings.hpp"
#include "database/StarredArtist.hpp"
#include "database/StarredRelease.hpp"
#include "database/StarredTrack.hpp"
#include "database/Track.hpp"
#include "database/TrackBookmark.hpp"
#include "database/TrackArtistLink.hpp"
#include "database/TrackList.hpp"
#include "database/TrackFeatures.hpp"
#include "database/TransactionChecker.hpp"
#include "database/User.hpp"
#include "EnumSetTraits.hpp"
#include "PathTraits.hpp"
#include "Migration.hpp"

namespace lms::db
{
    WriteTransaction::WriteTransaction(core::RecursiveSharedMutex& mutex, Wt::Dbo::Session& session)
        : _lock{ mutex },
        _transaction{ session }
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
        _session.mapClass<Listen>("listen");
        _session.mapClass<MediaLibrary>("media_library");
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

    void Session::prepareTables()
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

        Migration::doDbMigration(*this);

        // Indexes
        {
            auto transaction{ createWriteTransaction() };
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
            _session.execute("CREATE INDEX IF NOT EXISTS release_type_name_idx ON release_type(name)");
            _session.execute("CREATE INDEX IF NOT EXISTS track_id_idx ON track(id)");
            _session.execute("CREATE INDEX IF NOT EXISTS track_path_idx ON track(file_path)");
            _session.execute("CREATE INDEX IF NOT EXISTS track_name_idx ON track(name)");
            _session.execute("CREATE INDEX IF NOT EXISTS track_name_nocase_idx ON track(name COLLATE NOCASE)");
            _session.execute("CREATE INDEX IF NOT EXISTS track_mbid_idx ON track(mbid)");
            _session.execute("CREATE INDEX IF NOT EXISTS track_recording_mbid_idx ON track(recording_mbid)");
            _session.execute("CREATE INDEX IF NOT EXISTS track_release_idx ON track(release_id)");
            _session.execute("CREATE INDEX IF NOT EXISTS track_file_last_write_idx ON track(file_last_write)");
            _session.execute("CREATE INDEX IF NOT EXISTS track_date_idx ON track(date)");
            _session.execute("CREATE INDEX IF NOT EXISTS track_year_idx ON track(year)");
            _session.execute("CREATE INDEX IF NOT EXISTS track_original_date_idx ON track(original_date)");
            _session.execute("CREATE INDEX IF NOT EXISTS track_original_year_idx ON track(original_year)");
            _session.execute("CREATE INDEX IF NOT EXISTS track_media_library_idx ON track(media_library_id)");
            _session.execute("CREATE INDEX IF NOT EXISTS tracklist_name_idx ON tracklist(name)");
            _session.execute("CREATE INDEX IF NOT EXISTS tracklist_user_idx ON tracklist(user_id)");
            _session.execute("CREATE INDEX IF NOT EXISTS track_features_track_idx ON track_features(track_id)");
            _session.execute("CREATE INDEX IF NOT EXISTS track_artist_link_artist_idx ON track_artist_link(artist_id)");
            _session.execute("CREATE INDEX IF NOT EXISTS track_artist_link_track_idx ON track_artist_link(track_id)");
            _session.execute("CREATE INDEX IF NOT EXISTS track_artist_link_type_idx ON track_artist_link(type)");
            _session.execute("CREATE INDEX IF NOT EXISTS track_artist_link_artist_type_idx ON track_artist_link(artist_id,type)");
            _session.execute("CREATE INDEX IF NOT EXISTS track_bookmark_user_idx ON track_bookmark(user_id)");
            _session.execute("CREATE INDEX IF NOT EXISTS track_bookmark_user_track_idx ON track_bookmark(user_id,track_id)");
            _session.execute("CREATE INDEX IF NOT EXISTS listen_backend_idx ON listen(backend)");
            _session.execute("CREATE INDEX IF NOT EXISTS listen_user_backend_idx ON listen(user_id,backend)");
            _session.execute("CREATE INDEX IF NOT EXISTS listen_track_user_backend_idx ON listen(track_id,user_id,backend)");
            _session.execute("CREATE INDEX IF NOT EXISTS listen_user_track_backend_date_time_idx ON listen(user_id,track_id,backend,date_time)");
            _session.execute("CREATE INDEX IF NOT EXISTS starred_artist_user_backend_idx ON starred_artist(user_id,backend)");
            _session.execute("CREATE INDEX IF NOT EXISTS starred_artist_artist_user_backend_idx ON starred_artist(artist_id,user_id,backend)");
            _session.execute("CREATE INDEX IF NOT EXISTS starred_release_user_backend_idx ON starred_release(user_id,backend)");
            _session.execute("CREATE INDEX IF NOT EXISTS starred_release_release_user_backend_idx ON starred_release(release_id,user_id,backend)");
            _session.execute("CREATE INDEX IF NOT EXISTS starred_track_user_backend_idx ON starred_track(user_id,backend)");
            _session.execute("CREATE INDEX IF NOT EXISTS starred_track_track_user_backend_idx ON starred_track(track_id,user_id,backend)");
        }

        // Singletons
        {
            auto uniqueTransaction{ createWriteTransaction() };
            ScanSettings::init(*this);
        }
    }

    void Session::refreshTracingLoggerStats()
    {
        auto* traceLogger{ core::Service<core::tracing::ITraceLogger>::get() };
        if (!traceLogger)
            return;

        auto& dbSession{ _db.getTLSSession() };
        auto transaction{ dbSession.createReadTransaction() };

        traceLogger->setMetadata("db_artist_count", std::to_string(db::Artist::getCount(dbSession)));
        traceLogger->setMetadata("db_cluster_count", std::to_string(db::Cluster::getCount(dbSession)));
        traceLogger->setMetadata("db_cluster_type_count", std::to_string(db::ClusterType::getCount(dbSession)));
        traceLogger->setMetadata("db_starred_artist_count", std::to_string(db::StarredArtist::getCount(dbSession)));
        traceLogger->setMetadata("db_starred_release_count", std::to_string(db::StarredRelease::getCount(dbSession)));
        traceLogger->setMetadata("db_starred_track_count", std::to_string(db::StarredTrack::getCount(dbSession)));
        traceLogger->setMetadata("db_track_bookmark_count", std::to_string(db::TrackBookmark::getCount(dbSession)));
        traceLogger->setMetadata("db_listen_count", std::to_string(db::Listen::getCount(dbSession)));
        traceLogger->setMetadata("db_release_count", std::to_string(db::Release::getCount(dbSession)));
        traceLogger->setMetadata("db_track_count", std::to_string(db::Track::getCount(dbSession)));
    }

    void Session::analyze()
    {
        LMS_SCOPED_TRACE_DETAILED("Database", "Analyze");
        LMS_LOG(DB, INFO, "Analyzing database...");
        {
            auto transaction{ createWriteTransaction() };
            _session.execute("ANALYZE");
        }
        LMS_LOG(DB, INFO, "Database Analyze complete");
    }

    void Session::optimize()
    {
        LMS_SCOPED_TRACE_DETAILED("Database", "Optimize");
        LMS_LOG(DB, INFO, "Optimizing database...");
        {
            auto transaction{ createWriteTransaction() };
            _session.execute("PRAGMA optimize");
        }
        LMS_LOG(DB, INFO, "Database optimizing complete");
    }

} // namespace lms::db
