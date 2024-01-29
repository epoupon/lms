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

#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <boost/asio/io_service.hpp>
#include <Wt/WDateTime.h>

#include "services/scrobbling/Listen.hpp"
#include "database/ArtistId.hpp"
#include "database/ClusterId.hpp"
#include "database/MediaLibraryId.hpp"
#include "database/ReleaseId.hpp"
#include "database/TrackId.hpp"
#include "database/Types.hpp"

namespace Database
{
    class Db;
}

namespace Scrobbling
{
    class IScrobblingService
    {
    public:
        virtual ~IScrobblingService() = default;

        // Scrobbling
        virtual void listenStarted(const Listen& listen) = 0;
        virtual void listenFinished(const Listen& listen, std::optional<std::chrono::seconds> playedDuration = std::nullopt) = 0;

        virtual void addTimedListen(const TimedListen& listen) = 0;

        // Stats
        using ArtistContainer = Database::RangeResults<Database::ArtistId>;
        using ReleaseContainer = Database::RangeResults<Database::ReleaseId>;
        using TrackContainer = Database::RangeResults<Database::TrackId>;

        struct FindParameters
        {
            Database::UserId                              user;
            std::vector<Database::ClusterId>              clusters;	// if non empty, at least one artist that belongs to these clusters
            std::optional<Database::Range>                range;
            Database::MediaLibraryId                      library; // if set, match this library
            Database::ArtistId                            artist; // if set, match this artist

            FindParameters& setUser(const Database::UserId _user) { user = _user; return *this; }
            FindParameters& setClusters(const std::vector<Database::ClusterId>& _clusters) { clusters = _clusters; return *this; }
            FindParameters& setRange(std::optional<Database::Range> _range) { range = _range; return *this; }
            FindParameters& setMediaLibrary(Database::MediaLibraryId _library) { library = _library; return *this; }
            FindParameters& setArtist(Database::ArtistId _artist) { artist = _artist; return *this; }
        };

        // Artists
        struct ArtistFindParameters : public FindParameters
        {
            std::optional<Database::TrackArtistLinkType>  linkType;	// if set, only artists that have produced at least one track with this link type
            Database::ArtistSortMethod                    sortMethod{ Database::ArtistSortMethod::None };

            ArtistFindParameters& setLinkType(std::optional<Database::TrackArtistLinkType> _linkType) { linkType = _linkType; return *this; }
            ArtistFindParameters& setSortMethod(Database::ArtistSortMethod _sortMethod) { sortMethod = _sortMethod; return *this; }
        };

        // From most recent to oldest
        virtual ArtistContainer getRecentArtists(const ArtistFindParameters& params) = 0;
        virtual ReleaseContainer getRecentReleases(const FindParameters& params) = 0;
        virtual TrackContainer getRecentTracks(const FindParameters& params) = 0;

        virtual std::size_t getCount(Database::UserId userId, Database::ReleaseId releaseId) = 0;
        virtual std::size_t getCount(Database::UserId userId, Database::TrackId trackId) = 0;

        virtual Wt::WDateTime getLastListenDateTime(Database::UserId userId, Database::ReleaseId releaseId) = 0;
        virtual Wt::WDateTime getLastListenDateTime(Database::UserId userId, Database::TrackId trackId) = 0;

        // Top
        virtual ArtistContainer getTopArtists(const ArtistFindParameters& params) = 0;
        virtual ReleaseContainer getTopReleases(const FindParameters& params) = 0;
        virtual TrackContainer getTopTracks(const FindParameters& params) = 0;
    };

    std::unique_ptr<IScrobblingService> createScrobblingService(boost::asio::io_service& ioService, Database::Db& db);
} // ns Scrobbling
