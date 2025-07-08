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

#include <memory>
#include <unordered_map>

#include "services/feedback/IFeedbackService.hpp"

#include "IFeedbackBackend.hpp"

namespace lms::db
{
    class IDb;
}

namespace lms::feedback
{
    class FeedbackService : public IFeedbackService
    {
    public:
        FeedbackService(boost::asio::io_context& ioContext, db::IDb& db);
        ~FeedbackService() override;
        FeedbackService(const FeedbackService&) = delete;
        FeedbackService& operator=(const FeedbackService&) = delete;

    private:
        void star(db::UserId userId, db::ArtistId artistId) override;
        void unstar(db::UserId userId, db::ArtistId artistId) override;
        bool isStarred(db::UserId userId, db::ArtistId artistId) override;
        Wt::WDateTime getStarredDateTime(db::UserId userId, db::ArtistId artistId) override;
        ArtistContainer findStarredArtists(const ArtistFindParameters& params) override;

        void setRating(db::UserId userId, db::ArtistId artistId, std::optional<db::Rating> rating) override;
        std::optional<db::Rating> getRating(db::UserId userId, db::ArtistId artistId) override;

        void star(db::UserId userId, db::ReleaseId releaseId) override;
        void unstar(db::UserId userId, db::ReleaseId releaseId) override;
        bool isStarred(db::UserId userId, db::ReleaseId releaseId) override;
        Wt::WDateTime getStarredDateTime(db::UserId userId, db::ReleaseId releaseId) override;
        ReleaseContainer findStarredReleases(const FindParameters& params) override;

        void setRating(db::UserId userId, db::ReleaseId releaseId, std::optional<db::Rating> rating) override;
        std::optional<db::Rating> getRating(db::UserId userId, db::ReleaseId releaseId) override;

        void star(db::UserId userId, db::TrackId trackId) override;
        void unstar(db::UserId userId, db::TrackId trackId) override;
        bool isStarred(db::UserId userId, db::TrackId trackId) override;
        Wt::WDateTime getStarredDateTime(db::UserId userId, db::TrackId trackId) override;
        TrackContainer findStarredTracks(const FindParameters& params) override;

        void setRating(db::UserId userId, db::TrackId trackId, std::optional<db::Rating> rating) override;
        std::optional<db::Rating> getRating(db::UserId userId, db::TrackId trackId) override;

        std::optional<db::FeedbackBackend> getUserFeedbackBackend(db::UserId userId);

        template<typename ObjType, typename ObjIdType, typename StarredObjType>
        void star(db::UserId userId, ObjIdType id);
        template<typename ObjType, typename ObjIdType, typename StarredObjType>
        void unstar(db::UserId userId, ObjIdType id);
        template<typename ObjType, typename ObjIdType, typename StarredObjType>
        bool isStarred(db::UserId userId, ObjIdType id);
        template<typename ObjType, typename ObjIdType, typename StarredObjType>
        Wt::WDateTime getStarredDateTime(db::UserId userId, ObjIdType id);

        template<typename ObjType, typename ObjIdType, typename RatedObjType>
        void setRating(db::UserId userId, ObjIdType objectId, std::optional<db::Rating> rating);

        template<typename ObjType, typename ObjIdType, typename RatedObjType>
        std::optional<db::Rating> getRating(db::UserId userId, ObjIdType objectId);

        db::IDb& _db;
        std::unordered_map<db::FeedbackBackend, std::unique_ptr<IFeedbackBackend>> _backends;
    };

} // namespace lms::feedback
