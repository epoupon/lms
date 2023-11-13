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
#include <boost/asio/io_service.hpp>
#include <Wt/WDateTime.h>

#include "services/database/Types.hpp"
#include "services/database/ArtistId.hpp"
#include "services/database/ClusterId.hpp"
#include "services/database/ReleaseId.hpp"
#include "services/database/TrackId.hpp"
#include "services/database/UserId.hpp"
#include "services/database/Types.hpp"

namespace Database
{
    class Db;
}

namespace Feedback
{
    class IFeedbackService
    {
    public:
        virtual ~IFeedbackService() = default;

        using ArtistContainer = Database::RangeResults<Database::ArtistId>;
        using ReleaseContainer = Database::RangeResults<Database::ReleaseId>;
        using TrackContainer = Database::RangeResults<Database::TrackId>;

        struct FindParameters
        {
            Database::UserId                              user;
            std::vector<Database::ClusterId>              clusters;	// if non empty, at least one artist that belongs to these clusters
            std::optional<Database::Range>                range;

            FindParameters& setUser(const Database::UserId _user) { user = _user; return *this; }
            FindParameters& setClusters(const std::vector<Database::ClusterId>& _clusters) { clusters = _clusters; return *this; }
            FindParameters& setRange(std::optional<Database::Range> _range) { range = _range; return *this; }
        };

        // Artists
        struct ArtistFindParameters : public FindParameters
        {
            std::optional<Database::TrackArtistLinkType>  linkType;	// if set, only artists that have produced at least one track with this link type
            Database::ArtistSortMethod                    sortMethod{ Database::ArtistSortMethod::None };

            ArtistFindParameters& setLinkType(std::optional<Database::TrackArtistLinkType> _linkType) { linkType = _linkType; return *this; }
            ArtistFindParameters& setSortMethod(Database::ArtistSortMethod _sortMethod) { sortMethod = _sortMethod; return *this; }
        };

        virtual void                star(Database::UserId userId, Database::ArtistId artistId) = 0;
        virtual void                unstar(Database::UserId userId, Database::ArtistId artistId) = 0;
        virtual bool                isStarred(Database::UserId userId, Database::ArtistId artistId) = 0;
        virtual Wt::WDateTime       getStarredDateTime(Database::UserId userId, Database::ArtistId artistId) = 0;
        virtual ArtistContainer	    findStarredArtists(const ArtistFindParameters& params) = 0;

        // Releases
        virtual void                star(Database::UserId userId, Database::ReleaseId releaseId) = 0;
        virtual void                unstar(Database::UserId userId, Database::ReleaseId releaseId) = 0;
        virtual bool                isStarred(Database::UserId userId, Database::ReleaseId artistId) = 0;
        virtual Wt::WDateTime       getStarredDateTime(Database::UserId userId, Database::ReleaseId artistId) = 0;
        virtual ReleaseContainer    findStarredReleases(const FindParameters& params) = 0;

        // Tracks
        virtual void                star(Database::UserId userId, Database::TrackId trackId) = 0;
        virtual void                unstar(Database::UserId userId, Database::TrackId trackId) = 0;
        virtual bool                isStarred(Database::UserId userId, Database::TrackId artistId) = 0;
        virtual Wt::WDateTime       getStarredDateTime(Database::UserId userId, Database::TrackId artistId) = 0;
        virtual TrackContainer      findStarredTracks(const FindParameters& params) = 0;
    };

    std::unique_ptr<IFeedbackService> createFeedbackService(boost::asio::io_service& ioService, Database::Db& db);

} // ns Feedback

