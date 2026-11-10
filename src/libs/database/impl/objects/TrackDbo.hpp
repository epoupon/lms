/*
 * Copyright (C) 2026 Emeric Poupon
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

#pragma once

#include <Wt/Dbo/Dbo.h>
#include <Wt/Dbo/Field.h>
#include <Wt/Dbo/collection.h>

#include "database/objects/Track.hpp"

#include "DboAccess.hpp"

namespace lms::db::dbo
{
    class Artwork;
    class Cluster;
    class Directory;
    class Genre;
    class Grouping;
    class Language;
    class MediaLibrary;
    class Medium;
    class Mood;
    class Movement;
    class Release;
    class TrackArtistLink;
    class TrackEmbeddedImageLink;
    class TrackLyrics;
    class Work;

    class Track final : public db::Track, public Wt::Dbo::Dbo<Track>
    {
    public:
        Track() = default;
        using db::Track::Track;

        void syncIds()
        {
            _id = id();
            _releaseId = _release.id();
            _mediumId = _medium.id();
            _mediaLibraryId = _mediaLibrary.id();
            _directoryId = _directory.id();
            _preferredArtworkId = _preferredArtwork.id();
            _preferredMediaArtworkId = _preferredMediaArtwork.id();
        }

        void applyIds();

        template<class Action>
        void persist(Action& a)
        {
            Wt::Dbo::field(a, _scanVersion, "scan_version");

            Wt::Dbo::field(a, _absoluteFilePath, "absolute_file_path");
            Wt::Dbo::field(a, _fileSize, "file_size");
            Wt::Dbo::field(a, _fileLastWrite, "file_last_write");
            Wt::Dbo::field(a, _fileAdded, "file_added");

            Wt::Dbo::field(a, _duration, "duration");
            Wt::Dbo::field(a, _container, "container");
            Wt::Dbo::field(a, _codec, "codec");
            Wt::Dbo::field(a, _bitrate, "bitrate");
            Wt::Dbo::field(a, _channelCount, "channel_count");
            Wt::Dbo::field(a, _sampleRate, "sample_rate");
            Wt::Dbo::field(a, _bitsPerSample, "bits_per_sample");
            Wt::Dbo::field(a, _replayGain, "replay_gain");

            Wt::Dbo::field(a, _trackNumber, "track_number");
            Wt::Dbo::field(a, _name, "name");
            Wt::Dbo::field(a, _date, "date");
            Wt::Dbo::field(a, _originalDate, "original_date");
            Wt::Dbo::field(a, _trackMBID, "mbid");
            Wt::Dbo::field(a, _recordingMBID, "recording_mbid");
            Wt::Dbo::field(a, _copyright, "copyright");
            Wt::Dbo::field(a, _copyrightURL, "copyright_url");
            Wt::Dbo::field(a, _artistDisplayName, "artist_display_name");
            Wt::Dbo::field(a, _comment, "comment"); // TODO: move in a dedicated table?
            Wt::Dbo::field(a, _advisory, "advisory");

            Wt::Dbo::belongsTo(a, _medium, "medium", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::belongsTo(a, _release, "release", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::belongsTo(a, _mediaLibrary, "media_library", Wt::Dbo::OnDeleteSetNull); // don't delete track on media library removal, we want to wait for the next scan to have a chance to migrate files
            Wt::Dbo::belongsTo(a, _directory, "directory", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::belongsTo(a, _preferredArtwork, "preferred_artwork", Wt::Dbo::OnDeleteSetNull);
            Wt::Dbo::belongsTo(a, _preferredMediaArtwork, "preferred_media_artwork", Wt::Dbo::OnDeleteSetNull);
            Wt::Dbo::hasMany(a, _movements, Wt::Dbo::ManyToOne, "track");
            Wt::Dbo::hasMany(a, _trackArtistLinks, Wt::Dbo::ManyToOne, "track");
            Wt::Dbo::hasMany(a, _clusters, Wt::Dbo::ManyToMany, "track_cluster", "", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::hasMany(a, _genres, Wt::Dbo::ManyToMany, "track_genre", "", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::hasMany(a, _groupings, Wt::Dbo::ManyToMany, "track_grouping", "", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::hasMany(a, _languages, Wt::Dbo::ManyToMany, "track_language", "", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::hasMany(a, _moods, Wt::Dbo::ManyToMany, "track_mood", "", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::hasMany(a, _works, Wt::Dbo::ManyToMany, "track_work", "", Wt::Dbo::OnDeleteCascade);
            Wt::Dbo::hasMany(a, _trackLyrics, Wt::Dbo::ManyToOne, "track");
            Wt::Dbo::hasMany(a, _embeddedImageLinks, Wt::Dbo::ManyToOne, "track");
        }

        Wt::Dbo::ptr<Medium> _medium;
        Wt::Dbo::ptr<Release> _release;
        Wt::Dbo::ptr<MediaLibrary> _mediaLibrary;
        Wt::Dbo::ptr<Directory> _directory;
        Wt::Dbo::ptr<Artwork> _preferredArtwork;
        Wt::Dbo::ptr<Artwork> _preferredMediaArtwork;
        Wt::Dbo::collection<Wt::Dbo::ptr<TrackArtistLink>> _trackArtistLinks;
        Wt::Dbo::collection<Wt::Dbo::ptr<Cluster>> _clusters;
        Wt::Dbo::collection<Wt::Dbo::ptr<Genre>> _genres;
        Wt::Dbo::collection<Wt::Dbo::ptr<Grouping>> _groupings;
        Wt::Dbo::collection<Wt::Dbo::ptr<Language>> _languages;
        Wt::Dbo::collection<Wt::Dbo::ptr<Mood>> _moods;
        Wt::Dbo::collection<Wt::Dbo::ptr<Work>> _works;
        Wt::Dbo::collection<Wt::Dbo::ptr<Movement>> _movements;
        Wt::Dbo::collection<Wt::Dbo::ptr<TrackLyrics>> _trackLyrics;
        Wt::Dbo::collection<Wt::Dbo::ptr<TrackEmbeddedImageLink>> _embeddedImageLinks;
    };
} // namespace lms::db::dbo

namespace lms::db
{
    template<>
    struct DboOf<Track>
    {
        using type = dbo::Track;
    };
} // namespace lms::db
