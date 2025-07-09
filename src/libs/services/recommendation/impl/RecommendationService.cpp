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

#include "RecommendationService.hpp"

#include <vector>

#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/ScanSettings.hpp"

#include "ClustersEngineCreator.hpp"
#include "FeaturesEngineCreator.hpp"

namespace lms::recommendation
{
    namespace
    {
        db::ScanSettings::SimilarityEngineType getSimilarityEngineType(db::Session& session)
        {
            auto transaction{ session.createReadTransaction() };

            return db::ScanSettings::find(session)->getSimilarityEngineType();
        }
    } // namespace

    std::unique_ptr<IRecommendationService> createRecommendationService(db::IDb& db)
    {
        return std::make_unique<RecommendationService>(db);
    }

    RecommendationService::RecommendationService(db::IDb& db)
        : _db{ db }
    {
        load();
    }

    TrackContainer RecommendationService::findSimilarTracks(db::TrackListId trackListId, std::size_t maxCount) const
    {
        TrackContainer res;

        if (!_engine)
            return res;

        return _engine->findSimilarTracksFromTrackList(trackListId, maxCount);
    }

    TrackContainer RecommendationService::findSimilarTracks(const std::vector<db::TrackId>& trackIds, std::size_t maxCount) const
    {
        TrackContainer res;

        if (!_engine)
            return res;

        return _engine->findSimilarTracks(trackIds, maxCount);
    }

    ReleaseContainer RecommendationService::getSimilarReleases(db::ReleaseId releaseId, std::size_t maxCount) const
    {
        ReleaseContainer res;

        if (!_engine)
            return res;

        return _engine->getSimilarReleases(releaseId, maxCount);
        ;
    }

    ArtistContainer RecommendationService::getSimilarArtists(db::ArtistId artistId, core::EnumSet<db::TrackArtistLinkType> linkTypes, std::size_t maxCount) const
    {
        ArtistContainer res;

        if (!_engine)
            return res;

        return _engine->getSimilarArtists(artistId, linkTypes, maxCount);

        return res;
    }

    void RecommendationService::load()
    {
        using namespace db;

        switch (getSimilarityEngineType(_db.getTLSSession()))
        {
        case ScanSettings::SimilarityEngineType::Clusters:
            if (_engineType != EngineType::Clusters)
            {
                _engineType = EngineType::Clusters;
                _engine = createClustersEngine(_db);
            }
            break;

        case ScanSettings::SimilarityEngineType::Features:
        case ScanSettings::SimilarityEngineType::None:
            _engineType.reset();
            _engine.reset();
            break;
        }

        if (_engine)
            _engine->load(false);
    }
} // namespace lms::recommendation
