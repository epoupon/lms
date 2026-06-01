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

#include <memory>
#include <shared_mutex>

#include <boost/asio/io_context.hpp>

#include "core/IOContextRunner.hpp"

#include "database/objects/ScanSettings.hpp"
#include "services/recommendation/IRecommendationService.hpp"

#include "IEngine.hpp"

namespace lms::db
{
    class IDb;
}

namespace lms::recommendation
{
    class RecommendationService : public IRecommendationService
    {
    public:
        RecommendationService(db::IDb& db);
        ~RecommendationService() override = default;
        RecommendationService(const RecommendationService&) = delete;
        RecommendationService& operator=(const RecommendationService&) = delete;

    private:
        bool isEngineTypeSupported(EngineType type) const override;

        void requestReload() override;
        bool isLoaded() const override;
        EngineType getEngineType() const override;

        TrackResults findSimilarTracks(db::TrackListId tracklistId, std::size_t maxCount) const override;
        TrackResults findSimilarTracks(std::span<const db::TrackId> trackIds, std::size_t maxCount) const override;
        ReleaseResults findSimilarReleases(db::ReleaseId releaseId, std::size_t maxCount) const override;
        ArtistResults findSimilarArtists(db::ArtistId artistId, core::EnumSet<db::TrackArtistLinkType> linkTypes, std::size_t maxCount) const override;
        TrackResults findTrackSimilarityPath(db::TrackId startTrackId, db::TrackId endTrackId, std::size_t maxCount) const override;

        db::ScanSettings::RecommendationEngineType prepareReload();

        db::IDb& _db;
        mutable std::shared_mutex _mutex;
        EngineType _engineType{ EngineType::None };
        std::unique_ptr<IEngine> _engine;
        boost::asio::io_context _ioContext;
        core::IOContextRunner _ioContextRunner;
    };

} // namespace lms::recommendation
