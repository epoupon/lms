/*
 * Copyright (C) 2023 Emeric Poupon
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
#include "database/UserId.hpp"

namespace lms::db
{
    class Db;
}

namespace lms::feedback
{
    class IFeedbackService
    {
    public:
        virtual ~IFeedbackService() = default;

        using ArtistContainer = db::RangeResults<db::ArtistId>;
        using ReleaseContainer = db::RangeResults<db::ReleaseId>;
        using TrackContainer = db::RangeResults<db::TrackId>;

        struct FindParameters
        {
            db::UserId user;
            std::vector<db::ClusterId> clusters;    // if non empty, at least one artist that belongs to these clusters
            std::vector<std::string_view> keywords; // if non empty, name must match all of these keywords
            std::optional<db::Range> range;
            db::MediaLibraryId library;

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

        virtual void star(db::UserId userId, db::ArtistId artistId) = 0;
        virtual void unstar(db::UserId userId, db::ArtistId artistId) = 0;
        virtual bool isStarred(db::UserId userId, db::ArtistId artistId) = 0;
        virtual Wt::WDateTime getStarredDateTime(db::UserId userId, db::ArtistId artistId) = 0;
        virtual ArtistContainer findStarredArtists(const ArtistFindParameters& params) = 0;

        // Releases
        virtual void star(db::UserId userId, db::ReleaseId releaseId) = 0;
        virtual void unstar(db::UserId userId, db::ReleaseId releaseId) = 0;
        virtual bool isStarred(db::UserId userId, db::ReleaseId artistId) = 0;
        virtual Wt::WDateTime getStarredDateTime(db::UserId userId, db::ReleaseId artistId) = 0;
        virtual ReleaseContainer findStarredReleases(const FindParameters& params) = 0;

        // Tracks
        virtual void star(db::UserId userId, db::TrackId trackId) = 0;
        virtual void unstar(db::UserId userId, db::TrackId trackId) = 0;
        virtual bool isStarred(db::UserId userId, db::TrackId artistId) = 0;
        virtual Wt::WDateTime getStarredDateTime(db::UserId userId, db::TrackId artistId) = 0;
        virtual TrackContainer findStarredTracks(const FindParameters& params) = 0;
    };

    std::unique_ptr<IFeedbackService> createFeedbackService(boost::asio::io_service& ioService, db::Db& db);

} // namespace lms::feedback
