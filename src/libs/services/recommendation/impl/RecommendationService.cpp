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

#include "database/IDb.hpp"
#include "database/Session.hpp"
#include "database/objects/ScanSettings.hpp"

#include "audio-similarity/features/AudioFeatureEngine.hpp"
#include "audio-similarity/musicnn/MusicNNEmbeddingEngine.hpp"
#include "clusters/ClustersEngine.hpp"

namespace lms::recommendation
{
    namespace
    {
        db::ScanSettings::RecommendationEngineType getRecommendationEngineType(db::Session& session)
        {
            auto transaction{ session.createReadTransaction() };

            return db::ScanSettings::find(session)->getRecommendationEngineType();
        }
    } // namespace

    std::unique_ptr<IRecommendationService> createRecommendationService(db::IDb& db)
    {
        return std::make_unique<RecommendationService>(db);
    }

    RecommendationService::RecommendationService(db::IDb& db)
        : _db{ db }
    {
        requestReload();
    }

    TrackResults RecommendationService::findSimilarTracks(db::TrackListId trackListId, std::size_t maxCount) const
    {
        TrackResults res;

        if (!_engine)
            return res;

        return _engine->findSimilarTracksFromTrackList(trackListId, maxCount);
    }

    TrackResults RecommendationService::findSimilarTracks(std::span<const db::TrackId> trackIds, std::size_t maxCount) const
    {
        TrackResults res;

        if (!_engine)
            return res;

        return _engine->findSimilarTracks(trackIds, maxCount);
    }

    ReleaseResults RecommendationService::findSimilarReleases(db::ReleaseId releaseId, std::size_t maxCount) const
    {
        ReleaseResults res;

        if (!_engine)
            return res;

        return _engine->findSimilarReleases(releaseId, maxCount);
    }

    ArtistResults RecommendationService::findSimilarArtists(db::ArtistId artistId, core::EnumSet<db::TrackArtistLinkType> linkTypes, std::size_t maxCount) const
    {
        ArtistResults res;

        if (!_engine)
            return res;

        return _engine->findSimilarArtists(artistId, linkTypes, maxCount);
    }

    void RecommendationService::requestReload()
    {
        // not thread safe :/
        _engine.reset(); // may block

        switch (getRecommendationEngineType(_db.getTLSSession()))
        {
        case db::ScanSettings::RecommendationEngineType::Clusters:
            _engine = std::make_unique<ClusterEngine>(_db);
            break;

        case db::ScanSettings::RecommendationEngineType::AudioFeatures:
            _engine = std::make_unique<AudioFeatureEngine>(_db);
            break;

        case db::ScanSettings::RecommendationEngineType::AudioEmbeddings:
            _engine = std::make_unique<MusicNNEmbeddingEngine>(_db);
            break;

        case db::ScanSettings::RecommendationEngineType::None:
            _engine.reset();
            break;
        }

        if (_engine)
            _engine->requestReload();
    }

    bool RecommendationService::isLoaded() const
    {
        if (!_engine)
            return false;

        return _engine->isLoaded();
    }
} // namespace lms::recommendation
