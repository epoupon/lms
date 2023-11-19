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

#include <unordered_map>
#include <vector>

#include "ClustersEngineCreator.hpp"
#include "FeaturesEngineCreator.hpp"

#include "services/database/Db.hpp"
#include "services/database/Session.hpp"
#include "services/database/ScanSettings.hpp"
#include "utils/Exception.hpp"
#include "utils/Logger.hpp"

namespace Recommendation
{
    namespace
    {
        Database::ScanSettings::SimilarityEngineType getSimilarityEngineType(Database::Session& session)
        {
            auto transaction{ session.createReadTransaction() };

            return Database::ScanSettings::get(session)->getSimilarityEngineType();
        }
    }

    std::unique_ptr<IRecommendationService> createRecommendationService(Database::Db& db)
    {
        return std::make_unique<RecommendationService>(db);
    }

    RecommendationService::RecommendationService(Database::Db& db)
        : _db{ db }
    {
        load();
    }

    TrackContainer RecommendationService::findSimilarTracks(Database::TrackListId trackListId, std::size_t maxCount) const
    {
        TrackContainer res;

        if (!_engine)
            return res;

        return _engine->findSimilarTracksFromTrackList(trackListId, maxCount);
    }

    TrackContainer RecommendationService::findSimilarTracks(const std::vector<Database::TrackId>& trackIds, std::size_t maxCount) const
    {
        TrackContainer res;

        if (!_engine)
            return res;

        return _engine->findSimilarTracks(trackIds, maxCount);
    }

    ReleaseContainer RecommendationService::getSimilarReleases(Database::ReleaseId releaseId, std::size_t maxCount) const
    {
        ReleaseContainer res;

        if (!_engine)
            return res;

        return _engine->getSimilarReleases(releaseId, maxCount);;
    }

    ArtistContainer RecommendationService::getSimilarArtists(Database::ArtistId artistId, EnumSet<Database::TrackArtistLinkType> linkTypes, std::size_t maxCount) const
    {
        ArtistContainer res;

        if (!_engine)
            return res;

        return _engine->getSimilarArtists(artistId, linkTypes, maxCount);

        return res;
    }

    void RecommendationService::load()
    {
        using namespace Database;

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
} // ns Similarity
