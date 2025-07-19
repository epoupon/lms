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

#include <Wt/WDateTime.h>
#include <boost/asio/io_context.hpp>

#include "database/Types.hpp"
#include "database/objects/ArtistId.hpp"
#include "database/objects/Filters.hpp"
#include "database/objects/ReleaseId.hpp"
#include "database/objects/TrackId.hpp"
#include "database/objects/UserId.hpp"

namespace lms::db
{
    class IDb;
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
            db::Filters filters;
            std::vector<std::string_view> keywords; // if non empty, name must match all of these keywords
            std::optional<db::Range> range;

            FindParameters& setUser(const db::UserId _user)
            {
                user = _user;
                return *this;
            }
            FindParameters& setFilters(const db::Filters& _filters)
            {
                filters = _filters;
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

        virtual void setRating(db::UserId userId, db::ArtistId artistId, std::optional<db::Rating> rating) = 0;
        virtual std::optional<db::Rating> getRating(db::UserId userId, db::ArtistId artistId) = 0;

        // Releases
        virtual void star(db::UserId userId, db::ReleaseId releaseId) = 0;
        virtual void unstar(db::UserId userId, db::ReleaseId releaseId) = 0;
        virtual bool isStarred(db::UserId userId, db::ReleaseId artistId) = 0;
        virtual Wt::WDateTime getStarredDateTime(db::UserId userId, db::ReleaseId artistId) = 0;
        virtual ReleaseContainer findStarredReleases(const FindParameters& params) = 0;

        virtual void setRating(db::UserId userId, db::ReleaseId releaseId, std::optional<db::Rating> rating) = 0;
        virtual std::optional<db::Rating> getRating(db::UserId userId, db::ReleaseId releaseId) = 0;

        // Tracks
        virtual void star(db::UserId userId, db::TrackId trackId) = 0;
        virtual void unstar(db::UserId userId, db::TrackId trackId) = 0;
        virtual bool isStarred(db::UserId userId, db::TrackId artistId) = 0;
        virtual Wt::WDateTime getStarredDateTime(db::UserId userId, db::TrackId artistId) = 0;
        virtual TrackContainer findStarredTracks(const FindParameters& params) = 0;

        virtual void setRating(db::UserId userId, db::TrackId trackId, std::optional<db::Rating> rating) = 0;
        virtual std::optional<db::Rating> getRating(db::UserId userId, db::TrackId trackId) = 0;
    };

    std::unique_ptr<IFeedbackService> createFeedbackService(boost::asio::io_context& ioContext, db::IDb& db);

} // namespace lms::feedback
