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
#include <span>

#include <Wt/WDateTime.h>
#include <boost/asio/io_service.hpp>

#include "database/ArtistId.hpp"
#include "database/ClusterId.hpp"
#include "database/MediaLibraryId.hpp"
#include "database/ReleaseId.hpp"
#include "database/TrackId.hpp"
#include "database/Types.hpp"
#include "services/scrobbling/Listen.hpp"

namespace lms::db
{
    class Db;
}

namespace lms::scrobbling
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
        using ArtistContainer = db::RangeResults<db::ArtistId>;
        using ReleaseContainer = db::RangeResults<db::ReleaseId>;
        using TrackContainer = db::RangeResults<db::TrackId>;

        struct FindParameters
        {
            db::UserId user;
            std::vector<db::ClusterId> clusters;    // if non empty, at least one artist that belongs to these clusters
            std::vector<std::string_view> keywords; // if non empty, name must match all of these keywords
            std::optional<db::Range> range;
            db::MediaLibraryId library; // if set, match this library
            db::ArtistId artist;        // if set, match this artist

            FindParameters& setUser(const db::UserId _user)
            {
                user = _user;
                return *this;
            }
            FindParameters& setClusters(std::span<const db::ClusterId> _clusters)
            {
                clusters.assign(std::cbegin(_clusters), std::cend(_clusters));
                return *this;
            }
            FindParameters& setKeywords(const std::vector<std::string_view>& _keywords)
            {
                keywords = _keywords;
                return *this;
            }
            FindParameters& setRange(std::optional<db::Range> _range)
            {
                range = _range;
                return *this;
            }
            FindParameters& setMediaLibrary(db::MediaLibraryId _library)
            {
                library = _library;
                return *this;
            }
            FindParameters& setArtist(db::ArtistId _artist)
            {
                artist = _artist;
                return *this;
            }
        };

        // Artists
        struct ArtistFindParameters : public FindParameters
        {
            std::optional<db::TrackArtistLinkType> linkType; // if set, only artists that have produced at least one track with this link type
            db::ArtistSortMethod sortMethod{ db::ArtistSortMethod::None };

            ArtistFindParameters& setLinkType(std::optional<db::TrackArtistLinkType> _linkType)
            {
                linkType = _linkType;
                return *this;
            }
            ArtistFindParameters& setSortMethod(db::ArtistSortMethod _sortMethod)
            {
                sortMethod = _sortMethod;
                return *this;
            }
        };

        // From most recent to oldest
        virtual ArtistContainer getRecentArtists(const ArtistFindParameters& params) = 0;
        virtual ReleaseContainer getRecentReleases(const FindParameters& params) = 0;
        virtual TrackContainer getRecentTracks(const FindParameters& params) = 0;

        virtual std::size_t getCount(db::UserId userId, db::ReleaseId releaseId) = 0;
        virtual std::size_t getCount(db::UserId userId, db::TrackId trackId) = 0;

        virtual Wt::WDateTime getLastListenDateTime(db::UserId userId, db::ReleaseId releaseId) = 0;
        virtual Wt::WDateTime getLastListenDateTime(db::UserId userId, db::TrackId trackId) = 0;

        // Top
        virtual ArtistContainer getTopArtists(const ArtistFindParameters& params) = 0;
        virtual ReleaseContainer getTopReleases(const FindParameters& params) = 0;
        virtual TrackContainer getTopTracks(const FindParameters& params) = 0;
    };

    std::unique_ptr<IScrobblingService> createScrobblingService(boost::asio::io_service& ioService, db::Db& db);
} // namespace lms::scrobbling
