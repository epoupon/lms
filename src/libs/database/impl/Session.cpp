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

#include "Session.hpp"

#include <exception>

#include <Wt/Dbo/WtSqlTraits.h>

#include "ConnectionPool.hpp"
#include "Db.hpp"
#include "Migration.hpp"
#include "objects/ArtistDbo.hpp"
#include "objects/ArtistFeedbackBackendSyncDbo.hpp"
#include "objects/ArtistFeedbackDbo.hpp"
#include "objects/ArtistInfoDbo.hpp"
#include "objects/ArtworkDbo.hpp"
#include "objects/AuthTokenDbo.hpp"
#include "objects/ClusterDbo.hpp"
#include "objects/DirectoryDbo.hpp"
#include "objects/GenreDbo.hpp"
#include "objects/GroupingDbo.hpp"
#include "objects/ImageDbo.hpp"
#include "objects/LanguageDbo.hpp"
#include "objects/ListenBackendSyncDbo.hpp"
#include "objects/ListenDbo.hpp"
#include "objects/MediaLibraryDbo.hpp"
#include "objects/MediumDbo.hpp"
#include "objects/MoodDbo.hpp"
#include "objects/MovementDbo.hpp"
#include "objects/PlayListFileDbo.hpp"
#include "objects/PlayQueueDbo.hpp"
#include "objects/PodcastDbo.hpp"
#include "objects/PodcastEpisodeDbo.hpp"
#include "objects/RatedArtistDbo.hpp"
#include "objects/RatedReleaseDbo.hpp"
#include "objects/RatedTrackDbo.hpp"
#include "objects/ReleaseArtistLinkDbo.hpp"
#include "objects/ReleaseDbo.hpp"
#include "objects/ReleaseFeedbackBackendSyncDbo.hpp"
#include "objects/ReleaseFeedbackDbo.hpp"
#include "objects/ScanSettingsDbo.hpp"
#include "objects/ServerInfoDbo.hpp"
#include "objects/TrackArtistLinkDbo.hpp"
#include "objects/TrackBookmarkDbo.hpp"
#include "objects/TrackDbo.hpp"
#include "objects/TrackEmbeddedImageDbo.hpp"
#include "objects/TrackEmbeddedImageLinkDbo.hpp"
#include "objects/TrackFeedbackBackendSyncDbo.hpp"
#include "objects/TrackFeedbackDbo.hpp"
#include "objects/TrackListDbo.hpp"
#include "objects/TrackLyricsDbo.hpp"
#include "objects/TrackMusicNNEmbeddingsDbo.hpp"
#include "objects/UIStateDbo.hpp"
#include "objects/UserDbo.hpp"
#include "objects/WorkDbo.hpp"
#include "traits/EnumSetTraits.hpp"
#include "traits/ImageHashTypeTraits.hpp"
#include "traits/PartialDateTimeTraits.hpp"
#include "traits/PathTraits.hpp"
#include "traits/UUIDTraits.hpp"

namespace lms::db
{
    Session::Session(Db& db)
        : _db{ db }
    {
        _session.setConnectionPool(_db.getConnectionPool());

        _session.mapClass<dbo::Artist>("artist");
        _session.mapClass<dbo::ArtistInfo>("artist_info");
        _session.mapClass<dbo::Artwork>("artwork");
        _session.mapClass<dbo::AuthToken>("auth_token");
        _session.mapClass<dbo::Cluster>("cluster");
        _session.mapClass<dbo::ClusterType>("cluster_type");
        _session.mapClass<dbo::Genre>("genre");
        _session.mapClass<dbo::Grouping>("grouping");
        _session.mapClass<dbo::Language>("language");
        _session.mapClass<dbo::Mood>("mood");
        _session.mapClass<dbo::Country>("country");
        _session.mapClass<dbo::Directory>("directory");
        _session.mapClass<dbo::Image>("image");
        _session.mapClass<dbo::Label>("label");
        _session.mapClass<dbo::Listen>("listen");
        _session.mapClass<dbo::ListenBackendSync>("listen_backend_sync");
        _session.mapClass<dbo::MediaLibrary>("media_library");
        _session.mapClass<dbo::Medium>("medium");
        _session.mapClass<dbo::PlayListFile>("playlist_file");
        _session.mapClass<dbo::PlayQueue>("playqueue");
        _session.mapClass<dbo::Podcast>("podcast");
        _session.mapClass<dbo::PodcastEpisode>("podcast_episode");
        _session.mapClass<dbo::RatedArtist>("rated_artist");
        _session.mapClass<dbo::RatedRelease>("rated_release");
        _session.mapClass<dbo::RatedTrack>("rated_track");
        _session.mapClass<dbo::Release>("release");
        _session.mapClass<dbo::ReleaseArtistLink>("release_artist_link");
        _session.mapClass<dbo::ReleaseType>("release_type");
        _session.mapClass<dbo::ScanSettings>("scan_settings");
        _session.mapClass<dbo::ArtistFeedback>("artist_feedback");
        _session.mapClass<dbo::ArtistFeedbackBackendSync>("artist_feedback_backend_sync");
        _session.mapClass<dbo::ReleaseFeedback>("release_feedback");
        _session.mapClass<dbo::ReleaseFeedbackBackendSync>("release_feedback_backend_sync");
        _session.mapClass<dbo::TrackFeedback>("track_feedback");
        _session.mapClass<dbo::TrackFeedbackBackendSync>("track_feedback_backend_sync");
        _session.mapClass<dbo::Track>("track");
        _session.mapClass<dbo::Movement>("track_movement");
        _session.mapClass<dbo::TrackBookmark>("track_bookmark");
        _session.mapClass<dbo::TrackArtistLink>("track_artist_link");
        _session.mapClass<dbo::TrackEmbeddedImage>("track_embedded_image");
        _session.mapClass<dbo::TrackEmbeddedImageLink>("track_embedded_image_link");
        _session.mapClass<dbo::TrackMusicNNEmbeddings>("track_musicnn_embeddings");
        _session.mapClass<dbo::TrackList>("tracklist");
        _session.mapClass<dbo::TrackListEntry>("tracklist_entry");
        _session.mapClass<dbo::TrackLyrics>("track_lyrics");
        _session.mapClass<dbo::UIState>("ui_state");
        _session.mapClass<dbo::Work>("work");
        _session.mapClass<dbo::User>("user");
        _session.mapClass<dbo::ServerInfo>("server_info");
        _session.mapClass<VersionInfo>("version_info");
    }

    WriteTransaction Session::createWriteTransaction()
    {
        return WriteTransaction{ _db.getMutex(), *this };
    }

    ReadTransaction Session::createReadTransaction()
    {
        return ReadTransaction{ *this };
    }

    void Session::openTransaction()
    {
        _transactions.emplace_back(_session);
    }

    void Session::closeTransaction()
    {
        const bool unwinding{ std::uncaught_exceptions() != 0 };

        try
        {
            // Not left to the Wt destructor: pop_back is noexcept, a failed commit would terminate
            if (!unwinding)
            {
                const core::tracing::ScopedTrace trace{ "Database", core::tracing::Level::Detailed, "Commit" };
                _transactions.back().commit();
            }
        }
        catch (...)
        {
            _transactions.pop_back();
            _session.discardUnflushed();
            throw;
        }

        _transactions.pop_back();

        // Wt's rollback keeps modified objects pending: the next flush would write them
        if (unwinding)
            _session.discardUnflushed();
    }
} // namespace lms::db
