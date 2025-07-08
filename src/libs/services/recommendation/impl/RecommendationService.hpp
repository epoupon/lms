/*
 * Copyright (C) 2019 Emeric Poupon
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

#include <optional>

#include "services/recommendation/IRecommendationService.hpp"

#include "IEngine.hpp"

namespace lms::db
{
    class IDb;
}

namespace lms::recommendation
{
    enum class EngineType
    {
        Clusters,
        Features,
    };

    class RecommendationService : public IRecommendationService
    {
    public:
        RecommendationService(db::IDb& db);
        ~RecommendationService() override = default;
        RecommendationService(const RecommendationService&) = delete;
        RecommendationService& operator=(const RecommendationService&) = delete;

    private:
        void load() override;

        TrackContainer findSimilarTracks(db::TrackListId tracklistId, std::size_t maxCount) const override;
        TrackContainer findSimilarTracks(const std::vector<db::TrackId>& trackIds, std::size_t maxCount) const override;
        ReleaseContainer getSimilarReleases(db::ReleaseId releaseId, std::size_t maxCount) const override;
        ArtistContainer getSimilarArtists(db::ArtistId artistId, core::EnumSet<db::TrackArtistLinkType> linkTypes, std::size_t maxCount) const override;

        void setEnginePriorities(const std::vector<EngineType>& engineTypes);
        void clearEngines();
        void loadPendingEngine(EngineType engineType, std::unique_ptr<IEngine> engine, bool forceReload, const ProgressCallback& progressCallback);

        db::IDb& _db;
        std::optional<EngineType> _engineType;
        std::unique_ptr<IEngine> _engine;
    };

} // namespace lms::recommendation
