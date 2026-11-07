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

#include "database/objects/Artist.hpp"
#include "database/objects/ArtistFeedback.hpp"
#include "database/objects/ArtistFeedbackBackendSync.hpp"
#include "database/objects/ArtistInfo.hpp"
#include "database/objects/Artwork.hpp"
#include "database/objects/AuthToken.hpp"
#include "database/objects/Cluster.hpp"
#include "database/objects/Directory.hpp"
#include "database/objects/Genre.hpp"
#include "database/objects/Grouping.hpp"
#include "database/objects/Image.hpp"
#include "database/objects/Language.hpp"
#include "database/objects/Listen.hpp"
#include "database/objects/ListenBackendSync.hpp"
#include "database/objects/MediaLibrary.hpp"
#include "database/objects/Medium.hpp"
#include "database/objects/Mood.hpp"
#include "database/objects/Movement.hpp"
#include "database/objects/PlayListFile.hpp"
#include "database/objects/PlayQueue.hpp"
#include "database/objects/Podcast.hpp"
#include "database/objects/PodcastEpisode.hpp"
#include "database/objects/RatedArtist.hpp"
#include "database/objects/RatedRelease.hpp"
#include "database/objects/RatedTrack.hpp"
#include "database/objects/Release.hpp"
#include "database/objects/ReleaseArtistLink.hpp"
#include "database/objects/ReleaseFeedback.hpp"
#include "database/objects/ReleaseFeedbackBackendSync.hpp"
#include "database/objects/ScanSettings.hpp"
#include "database/objects/ServerInfo.hpp"
#include "database/objects/Track.hpp"
#include "database/objects/TrackArtistLink.hpp"
#include "database/objects/TrackBookmark.hpp"
#include "database/objects/TrackEmbeddedImage.hpp"
#include "database/objects/TrackEmbeddedImageLink.hpp"
#include "database/objects/TrackFeedback.hpp"
#include "database/objects/TrackFeedbackBackendSync.hpp"
#include "database/objects/TrackList.hpp"
#include "database/objects/TrackLyrics.hpp"
#include "database/objects/TrackMusicNNEmbeddings.hpp"
#include "database/objects/UIState.hpp"
#include "database/objects/User.hpp"
#include "database/objects/Work.hpp"

#include "ConnectionPool.hpp"
#include "Db.hpp"
#include "Migration.hpp"
#include "TransactionChecker.hpp"
#include "Utils.hpp"
#include "traits/EnumSetTraits.hpp"
#include "traits/ImageHashTypeTraits.hpp"
#include "traits/PartialDateTimeTraits.hpp"
#include "traits/PathTraits.hpp"
#include "traits/UUIDTraits.hpp"

namespace lms::db
{
    Session::Session(IDb& db)
        : _db{ static_cast<Db&>(db) }
    {
        _session.setConnectionPool(_db.getConnectionPool());

        _session.mapClass<Artist>("artist");
        _session.mapClass<ArtistInfo>("artist_info");
        _session.mapClass<Artwork>("artwork");
        _session.mapClass<AuthToken>("auth_token");
        _session.mapClass<Cluster>("cluster");
        _session.mapClass<ClusterType>("cluster_type");
        _session.mapClass<Genre>("genre");
        _session.mapClass<Grouping>("grouping");
        _session.mapClass<Language>("language");
        _session.mapClass<Mood>("mood");
        _session.mapClass<Country>("country");
        _session.mapClass<Directory>("directory");
        _session.mapClass<Image>("image");
        _session.mapClass<Label>("label");
        _session.mapClass<Listen>("listen");
        _session.mapClass<ListenBackendSync>("listen_backend_sync");
        _session.mapClass<MediaLibrary>("media_library");
        _session.mapClass<Medium>("medium");
        _session.mapClass<PlayListFile>("playlist_file");
        _session.mapClass<PlayQueue>("playqueue");
        _session.mapClass<Podcast>("podcast");
        _session.mapClass<PodcastEpisode>("podcast_episode");
        _session.mapClass<RatedArtist>("rated_artist");
        _session.mapClass<RatedRelease>("rated_release");
        _session.mapClass<RatedTrack>("rated_track");
        _session.mapClass<Release>("release");
        _session.mapClass<ReleaseArtistLink>("release_artist_link");
        _session.mapClass<ReleaseType>("release_type");
        _session.mapClass<ScanSettings>("scan_settings");
        _session.mapClass<ArtistFeedback>("artist_feedback");
        _session.mapClass<ArtistFeedbackBackendSync>("artist_feedback_backend_sync");
        _session.mapClass<ReleaseFeedback>("release_feedback");
        _session.mapClass<ReleaseFeedbackBackendSync>("release_feedback_backend_sync");
        _session.mapClass<TrackFeedback>("track_feedback");
        _session.mapClass<TrackFeedbackBackendSync>("track_feedback_backend_sync");
        _session.mapClass<Track>("track");
        _session.mapClass<Movement>("track_movement");
        _session.mapClass<TrackBookmark>("track_bookmark");
        _session.mapClass<TrackArtistLink>("track_artist_link");
        _session.mapClass<TrackEmbeddedImage>("track_embedded_image");
        _session.mapClass<TrackEmbeddedImageLink>("track_embedded_image_link");
        _session.mapClass<TrackMusicNNEmbeddings>("track_musicnn_embeddings");
        _session.mapClass<TrackList>("tracklist");
        _session.mapClass<TrackListEntry>("tracklist_entry");
        _session.mapClass<TrackLyrics>("track_lyrics");
        _session.mapClass<UIState>("ui_state");
        _session.mapClass<Work>("work");
        _session.mapClass<User>("user");
        _session.mapClass<ServerInfo>("server_info");
        _session.mapClass<VersionInfo>("version_info");
    }

    WriteTransaction Session::createWriteTransaction()
    {
        return WriteTransaction{ _db.getMutex(), _session };
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

    void Session::executeWithId(std::string_view query, long long id)
    {
        utils::executeCommand(_session, query, id);
    }

} // namespace lms::db
