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

#include "services/database/Session.hpp"

#include <cassert>

#include "utils/Exception.hpp"
#include "utils/Logger.hpp"

#include "services/database/Artist.hpp"
#include "services/database/AuthToken.hpp"
#include "services/database/Cluster.hpp"
#include "services/database/Db.hpp"
#include "services/database/Listen.hpp"
#include "services/database/Release.hpp"
#include "services/database/ScanSettings.hpp"
#include "services/database/StarredArtist.hpp"
#include "services/database/StarredRelease.hpp"
#include "services/database/StarredTrack.hpp"
#include "services/database/Track.hpp"
#include "services/database/TrackBookmark.hpp"
#include "services/database/TrackArtistLink.hpp"
#include "services/database/TrackList.hpp"
#include "services/database/TrackFeatures.hpp"
#include "services/database/User.hpp"
#include "EnumSetTraits.hpp"
#include "Migration.hpp"

namespace Database
{

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
        _session.mapClass<Release>("release");
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

    UniqueTransaction::UniqueTransaction(RecursiveSharedMutex& mutex, Wt::Dbo::Session& session)
        : _lock{ mutex },
        _transaction{ session }
    {
    }

    SharedTransaction::SharedTransaction(RecursiveSharedMutex& mutex, Wt::Dbo::Session& session)
        : _lock{ mutex },
        _transaction{ session }
    {
    }

    void Session::checkUniqueLocked()
    {
        assert(_db.getMutex().isUniqueLocked());
    }

    void Session::checkSharedLocked()
    {
        assert(_db.getMutex().isSharedLocked());
    }

    UniqueTransaction Session::createUniqueTransaction()
    {
        return UniqueTransaction{ _db.getMutex(), _session };
    }

    SharedTransaction Session::createSharedTransaction()
    {
        return SharedTransaction{ _db.getMutex(), _session };
    }

    void Session::prepareTables()
    {
        LMS_LOG(DB, INFO) << "Preparing tables...";

        // Initial creation case
        try
        {
            _session.createTables();
            LMS_LOG(DB, INFO) << "Tables created";
        }
        catch (Wt::Dbo::Exception& e)
        {
            LMS_LOG(DB, DEBUG) << "Cannot create tables: " << e.what();
            if (std::string_view{ e.what() }.find("already exists") == std::string_view::npos)
            {
                LMS_LOG(DB, ERROR) << "Cannot create tables: " << e.what();
                throw e;
            }
        }

        Migration::doDbMigration(*this);

        // Indexes
        {
            auto uniqueTransaction{ createUniqueTransaction() };
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
            _session.execute("CREATE INDEX IF NOT EXISTS track_recording_mbid_idx ON track(recording_mbid)");
            _session.execute("CREATE INDEX IF NOT EXISTS track_release_idx ON track(release_id)");
            _session.execute("CREATE INDEX IF NOT EXISTS track_date_idx ON track(date)");
            _session.execute("CREATE INDEX IF NOT EXISTS track_original_date_idx ON track(original_date)");
            _session.execute("CREATE INDEX IF NOT EXISTS tracklist_name_idx ON tracklist(name)");
            _session.execute("CREATE INDEX IF NOT EXISTS tracklist_user_idx ON tracklist(user_id)");
            _session.execute("CREATE INDEX IF NOT EXISTS track_features_track_idx ON track_features(track_id)");
            _session.execute("CREATE INDEX IF NOT EXISTS track_artist_link_artist_idx ON track_artist_link(artist_id)");
            _session.execute("CREATE INDEX IF NOT EXISTS track_artist_link_track_idx ON track_artist_link(track_id)");
            _session.execute("CREATE INDEX IF NOT EXISTS track_artist_link_type_idx ON track_artist_link(type)");
            _session.execute("CREATE INDEX IF NOT EXISTS track_artist_link_artist_type_idx ON track_artist_link(artist_id,type)");
            _session.execute("CREATE INDEX IF NOT EXISTS track_bookmark_user_idx ON track_bookmark(user_id)");
            _session.execute("CREATE INDEX IF NOT EXISTS track_bookmark_user_track_idx ON track_bookmark(user_id,track_id)");
            _session.execute("CREATE INDEX IF NOT EXISTS listen_scrobbler_idx ON listen(scrobbler)");
            _session.execute("CREATE INDEX IF NOT EXISTS listen_user_scrobbler_idx ON listen(user_id,scrobbler)");
            _session.execute("CREATE INDEX IF NOT EXISTS listen_user_track_scrobbler_date_time_idx ON listen(user_id,track_id,scrobbler,date_time)");
            _session.execute("CREATE INDEX IF NOT EXISTS starred_artist_user_scrobbler_idx ON starred_artist(user_id,scrobbler)");
            _session.execute("CREATE INDEX IF NOT EXISTS starred_artist_artist_user_scrobbler_idx ON starred_artist(artist_id,user_id,scrobbler)");
            _session.execute("CREATE INDEX IF NOT EXISTS starred_release_user_scrobbler_idx ON starred_release(user_id,scrobbler)");
            _session.execute("CREATE INDEX IF NOT EXISTS starred_release_release_user_scrobbler_idx ON starred_release(release_id,user_id,scrobbler)");
            _session.execute("CREATE INDEX IF NOT EXISTS starred_track_user_scrobbler_idx ON starred_track(user_id,scrobbler)");
            _session.execute("CREATE INDEX IF NOT EXISTS starred_track_track_user_scrobbler_idx ON starred_track(track_id,user_id,scrobbler)");
        }

        // Initial settings tables
        {
            auto uniqueTransaction{ createUniqueTransaction() };

            ScanSettings::init(*this);
        }
    }

    void Session::analyze()
    {
        LMS_LOG(DB, INFO) << "Analyzing database...";
        {
            auto uniqueTransaction{ createUniqueTransaction() };
            _session.execute("ANALYZE");
        }
        LMS_LOG(DB, INFO) << "Database Analyze complete";
    }

} // namespace Database
